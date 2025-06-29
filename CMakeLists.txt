cmake_minimum_required(VERSION 3.28)
project(user_level_threads_lib)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(user_level_threads_lib
        src/Thread.cpp
        src/Thread.h
        src/uthreads.cpp
        src/uthreads.h)
