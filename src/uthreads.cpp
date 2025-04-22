//
// Created by Owner on 21/04/2025.
//
#include "uthreads.h"
#include <queue>
#include <iostream>
#include <setjmp.h>
#include "Thread.h"
#define SUCCESS 0
#define FAILURE -1
#define THREAD_LIBRARY_ERROR(msg) \
    fprintf(stderr, "thread library error: %s\n", msg)

static int current_tid=-1;
static int total_quantums=0;

class ThreadManager{
 private:
  int current_tid;                        // Current running thread ID
  int total_quantums;                     // Total number of quantums
  std::queue<Thread*> ready_queue;        // Ready threads queue
  Thread* threads[MAX_THREAD_NUM];       // Array of all threads

};



int uthread_init(int quantum_usecs) {
  if (quantum_usecs <= 0) {
    THREAD_LIBRARY_ERROR("Invalid quantum value");
    return FAILURE;
  }
//  if (MAX_THREAD_NUM <= 0){ //todo - validate
//    return -1;
//  }
  current_tid=0;
  total_quantums=1;


}

