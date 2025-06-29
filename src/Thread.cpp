#include "Thread.h"

#include <setjmp.h>
#include <signal.h>  // Needed for sigemptyset

#define STACK_SIZE 4096  // Stack size per thread (in bytes)

#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
#endif

Thread::Thread() :
        tid(0),
        state(ThreadState::RUNNING),
        total_quantums(1),
        entry_point(nullptr),
        sleep_time(0)
{}

Thread::Thread(int id, thread_entry_point entry) :
        tid(id),
        state(ThreadState::READY),
        total_quantums(0),
        entry_point(entry),
        sleep_time(0)
{
    address_t sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t)entry_point;
    sigsetjmp(env, 1);
    env->__jmpbuf[JB_SP] = translate_address(sp);
    env->__jmpbuf[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}

Thread::~Thread() = default;

int Thread::get_quantums() const { return total_quantums; }
int Thread::get_sleep_time() const { return sleep_time; }
ThreadState Thread::get_state() const { return state; }
void Thread::set_quantums(int q) { total_quantums = q; }
void Thread::set_sleep_time(int t) { sleep_time = t; }
void Thread::set_state(ThreadState s) { state = s; }
