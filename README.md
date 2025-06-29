# User-Level Threads Library (uthreads)

A user-level threading library implemented in C++ for educational and systems programming purposes. This project was developed as part of the Hebrew University OS course.

## Features
- Cooperative user-level threads (uthreads) with context switching
- Thread creation, termination, blocking, resuming, and sleeping
- Quantum-based scheduling using virtual timer interrupts
- Thread-safe API (signal blocking)
- Simple and clear C++ design with modern best practices

## Project Structure
- `uthreads.h` / `uthreads.cpp` — Main API and implementation
- `Thread.h` / `Thread.cpp` — Thread class and context management
- `test0_sanity.cpp` — Sanity test: spawns and terminates threads, prints quantum counts
- `test2_two_thread.cpp` — Two-thread test: checks context switching between main and spawned thread

## How to Build
```sh
cd src
make                # builds the library (libuthreads.a)
g++ -std=c++11 -Wall -Wextra -g -o test0_sanity test0_sanity.cpp uthreads.cpp Thread.cpp
./test0_sanity

g++ -std=c++11 -Wall -Wextra -g -o test2_two_thread test2_two_thread.cpp uthreads.cpp Thread.cpp
./test2_two_thread
```

## Example Output
### test0_sanity
```
Thread:m Number:(0) 0
Init Quantum num is: 1
m0 Quanta:1
m0 Quanta:2
m0 Quanta:3
m spawns f at (1) 1
m spawns g at (2) 2
f1 Quanta:1
...
```
### test2_two_thread
```
test2:
--------------
***0***
***1***
***0***
***1***
...
```

## Why This Project?
- Demonstrates low-level systems programming and context switching
- Clean, modern, and well-documented C++ code
- Includes working tests and example outputs
- Great for learning, interviews, and as a professional portfolio piece

---
Feel free to open issues or contribute!
