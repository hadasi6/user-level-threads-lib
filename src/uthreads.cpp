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
#include "Thread.cpp"

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
#define RUNNING 0
#define READY 1
#define BLOCKED 2
#define THREAD_LIBRARY_ERROR(msg) \
    fprintf(stderr, "thread library error: %s\n", msg)

static int total_quantums = 0;
static struct itimerval timer = {0};
Thread* current_thread;
int quantum_duration;
Thread* should_terminate;
bool end_process;
std::queue<Thread*> ready_queue;
std::set<Thread*> sleeping;
std::priority_queue<int> pq;
std::map<int, Thread*> threads;
sigset_t blocked_sets;
#define BLOCK_SIG sigprocmask(SIG_BLOCK, &blocked_sets,NULL)
#define ALLOW_SIG sigprocmask(SIG_UNBLOCK, &blocked_sets,NULL)

void reset_timer(int usecs) {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usecs;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = usecs;
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

void update_sleep() {
    std::vector<Thread*> to_wake;
    for (Thread* t : sleeping) {
        t->set_sleep_time(t->get_sleep_time() - 1);
        if (t->get_sleep_time() == 0) {
            to_wake.push_back(t);
        }
    }
    for (Thread* t : to_wake) {
        sleeping.erase(t);
        if (t->get_state() != BLOCKED) {
            ready_queue.push(t);
        }
    }
}

void clean_and_exit() {
    for (auto& pair : threads) {
        if (pair.first != 0) {
            delete pair.second;
            pair.second = nullptr;
        }
    }
    delete current_thread;
    current_thread = nullptr;
    threads.clear();
    exit(0);

}

void switch_thread() {
    BLOCK_SIG;
    update_sleep();

    //check if was sent to sleep
    if (!should_terminate&& current_thread->sleep_time <= 0) {
        current_thread->set_state(READY);
        ready_queue.push(current_thread);
    }

    if (ready_queue.empty()) {
        ALLOW_SIG;
        return;
    }

    Thread* prev = current_thread;

    current_thread = ready_queue.front();
    ready_queue.pop();
    current_thread->set_state(RUNNING);
    current_thread->set_quantums(current_thread->get_quantums() + 1);
    total_quantums++;

    if (!should_terminate) {
        if (sigsetjmp(prev->env, 1) == 0) {
            reset_timer(quantum_duration);
            ALLOW_SIG;
            siglongjmp(current_thread->env, 1);
        }
        else {
            if (end_process && current_thread->tid == 0) {
//                while (!ready_queue.empty()) ready_queue.pop();
//                sleeping.clear();
//                while (!pq.empty()) pq.pop();
                clean_and_exit();
            }
            if (should_terminate) {
                int id = should_terminate->tid;
                delete should_terminate;
                should_terminate = nullptr;
                threads.erase(id);
                reset_timer(quantum_duration);
            }
        }
    }
    else {
        ALLOW_SIG;
        siglongjmp(current_thread->env, 1);
    }
    ALLOW_SIG;
}

void timer_handler(int sig) {
    switch_thread();
}

void free_memory(){
    for (auto& pair : threads) {
        delete pair.second;
    }
    threads.clear();
    while (!ready_queue.empty()) ready_queue.pop();
    sleeping.clear();
    while (!pq.empty()) pq.pop();
}

int uthread_init(int quantum_usecs) {
    BLOCK_SIG;
    end_process = false;
    should_terminate= nullptr;
    sigemptyset(&blocked_sets);
    sigaddset(&blocked_sets, SIGVTALRM);
    if (quantum_usecs <= 0) {
        THREAD_LIBRARY_ERROR("Invalid quantum value");
        return FAILURE;
    }

    total_quantums = 1;
    quantum_duration = quantum_usecs;
    for (int i = 1; i < MAX_THREAD_NUM; i++) pq.push(-i);
    current_thread = new Thread();
    threads[0] = current_thread;

    struct sigaction sa = {0};
    sa.sa_handler = &timer_handler;
    sigaction(SIGVTALRM, &sa, nullptr);

//    switch_thread();
    reset_timer(quantum_duration);
//    switch_thread();
    ALLOW_SIG;
    return SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point) {
    BLOCK_SIG;
    if (pq.empty() || !entry_point) {
        THREAD_LIBRARY_ERROR("Unable to spawn thread");
        ALLOW_SIG;
        return FAILURE;
    }
    int id = -(pq.top());
    pq.pop();
    Thread* t = new Thread(id, entry_point);
    if (t == nullptr) {
        THREAD_LIBRARY_ERROR("Thread creation failed");
        ALLOW_SIG;
        return FAILURE;
    }
    ready_queue.push(t);
    threads[id] = t;
    ALLOW_SIG;
    return id;
}

int uthread_terminate(int tid) {
    BLOCK_SIG;
    if (threads.find(tid) == threads.end()) {
        THREAD_LIBRARY_ERROR("Thread ID does not exist");
        ALLOW_SIG;
        return FAILURE;
    }

    if (tid == 0) {
        if (current_thread->tid==0){
            clean_and_exit();
        }
        end_process = true;
//        ready_queue.empty();
        siglongjmp(threads[0]->env, 1);
    }

    Thread* to_delete = threads[tid];

    if (current_thread->tid == tid) {
        should_terminate = current_thread;
        ALLOW_SIG;
        switch_thread();
        return SUCCESS;
    }

    threads.erase(tid);
    sleeping.erase(to_delete);
    pq.push(-tid);
    delete to_delete;

    std::queue<Thread*> temp;
    while (!ready_queue.empty()) {
        Thread* curr = ready_queue.front();
        ready_queue.pop();
        if (curr->tid != tid) temp.push(curr);
    }
    ready_queue = temp;

    ALLOW_SIG;
    return SUCCESS;
}

int uthread_block(int tid) {
    BLOCK_SIG;
    if (tid == 0 || threads.find(tid) == threads.end()) {
        THREAD_LIBRARY_ERROR("Invalid operation");
        ALLOW_SIG;
        return FAILURE;
    }

    Thread* t = threads[tid];
    if (t->get_state() != BLOCKED) {
        t->set_state(BLOCKED);

        std::queue<Thread*> temp;
        while (!ready_queue.empty()) {
            Thread* curr = ready_queue.front();
            ready_queue.pop();
            if (curr->tid != tid) temp.push(curr);
        }
        ready_queue = temp;

        if (current_thread->tid == tid) {
            ALLOW_SIG;
            switch_thread();
            return SUCCESS;
        }
    }
    ALLOW_SIG;
    return SUCCESS;
}

int uthread_resume(int tid) {
    BLOCK_SIG;
    if (threads.find(tid) == threads.end()) {
        THREAD_LIBRARY_ERROR("Thread ID does not exist");
        ALLOW_SIG;
        return FAILURE;
    }

    Thread* t = threads[tid];
    if (t->get_state() == BLOCKED && sleeping.find(t) == sleeping.end()) {
        t->set_state(READY);
        ready_queue.push(t);
    }

    ALLOW_SIG;
    return SUCCESS;
}

int uthread_sleep(int num_quantums) {
    BLOCK_SIG;
    if (current_thread->tid == 0 || num_quantums < 0) {
        THREAD_LIBRARY_ERROR("Invalid sleep operation");
        ALLOW_SIG;
        return FAILURE;
    }

    current_thread->set_sleep_time(num_quantums+1);
    sleeping.insert(current_thread);
    ALLOW_SIG;
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
    BLOCK_SIG;
    if (threads.find(tid) == threads.end()) {
        THREAD_LIBRARY_ERROR("Thread ID does not exist");
        ALLOW_SIG;
        return FAILURE;
    }
    int quantums = threads[tid]->get_quantums();
    ALLOW_SIG;
    return quantums;
}

/*
int main(){
    uthread_init(100);
}*/
