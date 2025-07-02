#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include "uthreads.h"
#include <queue>
#include <set>
#include <map>
#include <iostream>
#include "Thread.h" 

#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}
#endif

#define SUCCESS 0
#define FAILURE -1

#define THREAD_LIBRARY_ERROR(msg) \
    fprintf(stderr, "thread library error: %s\n", msg)

#define SYSTEM_ERROR(msg) \
    fprintf(stderr, "system error: %s\n", msg)

// ================== Global Variables =====================
static int total_quantums = 0;
static struct itimerval timer = {0};
Thread* current_thread = nullptr;
int quantum_duration = 0;
Thread* should_terminate = nullptr;
bool end_process = false;
int exit_status = 0;
std::queue<Thread*> ready_queue;
std::set<Thread*> sleeping_threads;
std::priority_queue<int> available_ids;
std::map<int, Thread*> all_threads;
sigset_t blocked_sets;
#define BLOCK_TIMER_SIGNAL sigprocmask(SIG_BLOCK, &blocked_sets, nullptr)
#define UNBLOCK_TIMER_SIGNAL sigprocmask(SIG_UNBLOCK, &blocked_sets, nullptr)

/**
 * Clean up all threads and exit the process.
 * @param exit_code Exit status code.
 */
void clean_and_exit(int exit_code = 0) {
    for (auto& pair : all_threads) {
        if (pair.first != 0) {
            delete pair.second;
            pair.second = nullptr;
        }
    }
    if (current_thread) {
        delete current_thread;
        current_thread = nullptr;
    }
    all_threads.clear();
    exit(exit_code);
}

/**
 * Reset the virtual timer for thread quantums.
 * @param usecs Quantum duration in microseconds.
 */
void reset_timer(int usecs) {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usecs;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = usecs;
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) == -1) {
        SYSTEM_ERROR("setitimer failed");
        exit_status = 1;
        uthread_terminate(0);
    }
}

/**
 * Update all sleeping threads, waking those whose sleep time is over.
 */
void update_sleeping_threads() {
    std::vector<Thread*> to_wake;
    for (auto* thread : sleeping_threads) {
        thread->set_sleep_time(thread->get_sleep_time() - 1);
        if (thread->get_sleep_time() == 0) {
            to_wake.push_back(thread);
        }
    }
    for (auto* thread : to_wake) {
        sleeping_threads.erase(thread);
        if (thread->get_state() != ThreadState::BLOCKED) {
            ready_queue.push(thread);
        }
    }
}

/**
 * Finalize and clean up a thread marked for termination.
 */
void finalize_terminated_thread() {
    int tid = should_terminate->tid;
    delete should_terminate;
    should_terminate = nullptr;
    all_threads.erase(tid);
    available_ids.push(-tid);
    reset_timer(quantum_duration);
}

static void enqueue_current_if_needed() {
    if (!should_terminate && current_thread->sleep_time <= 0) {
        current_thread->set_state(ThreadState::READY);
        ready_queue.push(current_thread);
    }
}

static void handle_end_process() {
    if (end_process && current_thread->tid == 0) {
        while (!ready_queue.empty()) ready_queue.pop();
        sleeping_threads.clear();
        while (!available_ids.empty()) available_ids.pop();
        clean_and_exit(0);
    }
}

static void handle_should_terminate() {
    if (should_terminate) {
        finalize_terminated_thread();
    }
}

/**
 * Switch context to the next ready thread.
 * Handles sleeping, termination, and process end.
 */
void switch_thread() {
    BLOCK_TIMER_SIGNAL;
    update_sleeping_threads();
    enqueue_current_if_needed();  
    if (ready_queue.empty()) {
        UNBLOCK_TIMER_SIGNAL;
        return;
    }
    Thread* prev = current_thread;
    current_thread = ready_queue.front();
    ready_queue.pop();
    current_thread->set_state(ThreadState::RUNNING);
    current_thread->set_quantums(current_thread->get_quantums() + 1);
    total_quantums++;
    if (!should_terminate) {
        if (sigsetjmp(prev->env, 1) == 0) {
            reset_timer(quantum_duration);
            UNBLOCK_TIMER_SIGNAL;
            siglongjmp(current_thread->env, 1);
        }
        else {
            handle_end_process();
            handle_should_terminate();
        }
    }
    else {
        UNBLOCK_TIMER_SIGNAL;
        siglongjmp(current_thread->env, 1);
    }
    UNBLOCK_TIMER_SIGNAL;
}

void timer_handler(int sig) {
    switch_thread();
}

void free_memory(){
    for (auto& pair : all_threads) {
        delete pair.second;
    }
    all_threads.clear();
    while (!ready_queue.empty()) ready_queue.pop();
    sleeping_threads.clear();
    while (!available_ids.empty()) available_ids.pop();
}

static void init_signal_mask() {
    if (sigemptyset(&blocked_sets) == -1) {
        SYSTEM_ERROR("sigemptyset failed");
        exit_status = 1;
        clean_and_exit(exit_status);
    }
    if (sigaddset(&blocked_sets, SIGVTALRM) == -1) {
        SYSTEM_ERROR("sigaddset failed");
        exit_status = 1;
        clean_and_exit(exit_status);
    }
}

static void init_available_ids() {
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        available_ids.push(-i);
    }
}

static void init_main_thread() {
    try {
        current_thread = new Thread();
    } catch (const std::bad_alloc& e) {
        SYSTEM_ERROR("Thread creation failed");
        UNBLOCK_TIMER_SIGNAL;
        exit_status = 1;
        clean_and_exit(exit_status);
    }
    all_threads[0] = current_thread;
}

static void setup_timer_handler() {
    struct sigaction sa = {0};
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, nullptr) == -1) {
        SYSTEM_ERROR("sigaction failed");
        exit_status = 1;
        clean_and_exit(exit_status);
    }
}

int uthread_init(int quantum_usecs) {
    BLOCK_TIMER_SIGNAL;
    end_process = false;
    should_terminate = nullptr;
    exit_status = 0;

    init_signal_mask();
    if (quantum_usecs <= 0) {
        THREAD_LIBRARY_ERROR("Invalid quantum value");
        return FAILURE;
    }
    total_quantums = 1;
    quantum_duration = quantum_usecs;
    init_available_ids();
    init_main_thread();
    setup_timer_handler();
    reset_timer(quantum_duration);
    UNBLOCK_TIMER_SIGNAL;
    return SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point) {
    BLOCK_TIMER_SIGNAL;
    if (available_ids.empty() || !entry_point) {
        THREAD_LIBRARY_ERROR("Unable to spawn thread");
        UNBLOCK_TIMER_SIGNAL;
        return FAILURE;
    }
    int id = -(available_ids.top());
    available_ids.pop();
    Thread* t = nullptr;
    try {
        t = new Thread(id, entry_point);
    }
    catch (const std::bad_alloc& e) {
        SYSTEM_ERROR("Thread creation failed: bad_alloc");
        UNBLOCK_TIMER_SIGNAL;
        exit_status = 1;
        uthread_terminate(0);
    }
    ready_queue.push(t);
    all_threads[id] = t;
    UNBLOCK_TIMER_SIGNAL;
    return id;
}

/**
 * Remove a thread from the ready queue by its tid.
 * @param tid Thread ID to remove.
 */
static void remove_thread_from_ready_queue(const int tid) {
    std::queue<Thread*> temp;
    while (!ready_queue.empty()) {
        Thread* curr = ready_queue.front();
        ready_queue.pop();
        if (curr->tid != tid) temp.push(curr);
    }
    ready_queue = temp;
}

int uthread_terminate(int tid) {
    BLOCK_TIMER_SIGNAL;
    if (all_threads.find(tid) == all_threads.end()) {
        THREAD_LIBRARY_ERROR("Thread ID does not exist");
        UNBLOCK_TIMER_SIGNAL;
        return FAILURE;
    }

    if (tid == 0) {
        if (current_thread->tid==0){
            clean_and_exit(0);
        }
        end_process = true;
        siglongjmp(all_threads[0]->env, 1);
    }

    Thread* to_delete = all_threads[tid];
    if (current_thread->tid == tid) {
        should_terminate = current_thread;
        UNBLOCK_TIMER_SIGNAL;
        switch_thread();
        return SUCCESS;
    }

    all_threads.erase(tid);
    sleeping_threads.erase(to_delete);
    available_ids.push(-tid);
    delete to_delete;

    remove_thread_from_ready_queue(tid);
    UNBLOCK_TIMER_SIGNAL;
    return SUCCESS;
}

int uthread_block(int tid) {
    BLOCK_TIMER_SIGNAL;
    if (tid == 0 || all_threads.find(tid) == all_threads.end()) {
        THREAD_LIBRARY_ERROR("Invalid operation");
        UNBLOCK_TIMER_SIGNAL;
        return FAILURE;
    }
    Thread* t = all_threads[tid];
    if (t->get_state() != ThreadState::BLOCKED) {
        t->set_state(ThreadState::BLOCKED);
        std::queue<Thread*> temp;
        while (!ready_queue.empty()) {
            Thread* curr = ready_queue.front();
            ready_queue.pop();
            if (curr->tid != tid) temp.push(curr);
        }
        ready_queue = temp;
        if (current_thread->tid == tid) {
            UNBLOCK_TIMER_SIGNAL;
            switch_thread();
            return SUCCESS;
        }
    }
    UNBLOCK_TIMER_SIGNAL;
    return SUCCESS;
}

int uthread_resume(int tid) {
    BLOCK_TIMER_SIGNAL;
    if (all_threads.find(tid) == all_threads.end()) {
        THREAD_LIBRARY_ERROR("Thread ID does not exist");
        UNBLOCK_TIMER_SIGNAL;
        return FAILURE;
    }
    Thread* t = all_threads[tid];
    if (t->get_state() == ThreadState::BLOCKED && sleeping_threads.find(t) == sleeping_threads.end()) {
        t->set_state(ThreadState::READY);
        ready_queue.push(t);
    }
    UNBLOCK_TIMER_SIGNAL;
    return SUCCESS;
}

int uthread_sleep(int num_quantums) {
    BLOCK_TIMER_SIGNAL;
    if (current_thread->tid == 0 || num_quantums < 0) {
        THREAD_LIBRARY_ERROR("Invalid sleep operation");
        UNBLOCK_TIMER_SIGNAL;
        return FAILURE;
    }

    current_thread->set_sleep_time(num_quantums+1);
    sleeping_threads.insert(current_thread);
    UNBLOCK_TIMER_SIGNAL;
    switch_thread();
    return SUCCESS;
}

int uthread_get_tid() {
    return current_thread->tid;
}

int uthread_get_total_quantums() {
    return total_quantums;
}

int uthread_get_quantums(int tid) {
    BLOCK_TIMER_SIGNAL;
    if (all_threads.find(tid) == all_threads.end()) {
        THREAD_LIBRARY_ERROR("Thread ID does not exist");
        UNBLOCK_TIMER_SIGNAL;
        return FAILURE;
    }
    int quantums = all_threads[tid]->get_quantums();
    UNBLOCK_TIMER_SIGNAL;
    return quantums;
}