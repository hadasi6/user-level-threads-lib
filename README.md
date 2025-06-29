# User-Level Threads Library (uthreads)

A C++ user-level threading library supporting both preemptive and cooperative multithreading, with quantum-based scheduling and a clear API for thread management in user space.

Easily create, schedule, and manage multiple user-level threads in your application, with robust context switching and safe signal handling.

## Technical Highlights
- Implements user-level context switching using `setjmp`/`longjmp` and manual stack manipulation
- Uses Linux signals and virtual timers (`setitimer`, `SIGVTALRM`) for preemptive scheduling
- Thread management and scheduling logic is decoupled from application logic
- Emphasis on reliability, maintainability, and clear error handling

## Features
- User-level threads (uthreads) with context switching
- Thread creation, termination, blocking, resuming, and sleeping
- Quantum-based round-robin scheduling
- Signal-safe API using `sigprocmask` for thread safety

## Example Usage
```cpp
#include "uthreads.h"
#include <iostream>

void thread_func() {
    std::cout << "Hello from thread " << uthread_get_tid() << std::endl;
    uthread_terminate(uthread_get_tid());
}

int main() {
    uthread_init(1000);
    int tid = uthread_spawn(thread_func);
    std::cout << "Main thread, spawned thread id: " << tid << std::endl;
    while (true) {} // Let threads run
}
```

## Project Structure
- `uthreads.h` / `uthreads.cpp` — Main API and implementation
- `Thread.h` / `Thread.cpp` — Thread class and context management
- `examples/` — Usage examples and tests
- `tests/` — Expected outputs for validation

## Quick Start
```sh
cd src
make
g++ -std=c++11 -Wall -Wextra -g -o test0_sanity ../examples/test0_sanity.cpp uthreads.cpp Thread.cpp
./test0_sanity
```

## Design Notes
- Context switching is implemented with setjmp/longjmp for portability and control.
- Preemptive scheduling is achieved using Linux virtual timers and signals.
- All thread management is signal-safe to prevent race conditions and ensure robustness.

## Example Output
<details>
<summary>test0_sanity</summary>

```
Thread:m Number:(0) 0
Init Quantum num is: 1
m0 Quanta:1
...
```
</details>

<details>
<summary>test2_two_thread</summary>

```
test2:
--------------
***0***
***1***
...
```
</details>

---
For questions or feedback, feel free to open an issue or contact me via GitHub.
