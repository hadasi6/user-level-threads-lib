#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>
#include <signal.h>  // Needed for sigemptyset

#define STACK_SIZE 100000  // Stack size per thread (in bytes)

#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
#endif

#define RUNNING 0
#define READY 1
#define BLOCKED 2

typedef void (*thread_entry_point)(void);

address_t translate_address(address_t addr); // Declared elsewhere

class Thread {
public:
    int tid;
    int state;
    sigjmp_buf env;
    char stack[STACK_SIZE];
    int total_quantums;
    thread_entry_point entry_point;
    int sleep_time;

    // Constructor for main thread (tid == 0)
    Thread() :
            tid(0),
            state(RUNNING),
            total_quantums(1),
            entry_point(nullptr),
            sleep_time(0)
    {}

    // Constructor for all other threads
    Thread(int id, thread_entry_point entry) :
            tid(id),
            state(READY),
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

    ~Thread() = default;

    // Getters
    int get_quantums() const { return total_quantums; }
    int get_sleep_time() const { return sleep_time; }
    int get_state() const { return state; }

    // Setters
    void set_quantums(int q) { total_quantums = q; }
    void set_sleep_time(int t) { sleep_time = t; }
    void set_state(int s) { state = s; }
};

#endif // THREAD_H
