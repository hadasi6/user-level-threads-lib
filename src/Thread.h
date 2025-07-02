
#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>
#include <signal.h>

#define STACK_SIZE 4096

#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
#endif

// Thread state enum for clarity and type safety
/**
 * @brief Enum representing the possible states of a thread.
 */
enum class ThreadState {
    RUNNING = 0,
    READY = 1,
    BLOCKED = 2
};

typedef void (*thread_entry_point)(void);

address_t translate_address(address_t addr);  // Declared elsewhere

class Thread {
public:
    int tid;
    ThreadState state;
    sigjmp_buf env;
    char stack[STACK_SIZE];
    int total_quantums;
    thread_entry_point entry_point;
    int sleep_time;

    // Constructor for main thread
    Thread();

    // Constructor for other threads
    Thread(int id, thread_entry_point entry);

    ~Thread();

    // Getters
    int get_quantums() const;
    int get_sleep_time() const;
    ThreadState get_state() const;

    // Setters
    void set_quantums(int q);
    void set_sleep_time(int t);
    void set_state(ThreadState s);
};

#endif // THREAD_H
