#pragma once
#include "basic_struct/data_structures.h"
#include <pthread.h>

namespace rockcoro{

#define SCHEDULER_NUM_WORKERS 10
struct Coroutine;

typedef void (*CoroutineFunc)(void*);

struct Scheduler{
    /// @brief the job queue
    Queue<Coroutine*> job_queue;
    pthread_mutex_t mutex_job_queue;
    pthread_cond_t cond_job_queue;
    // workers
    pthread_t workers[SCHEDULER_NUM_WORKERS];

    Scheduler();
    ~Scheduler();
    /// @brief gets the scheduler instance
    static Scheduler& get();
    static void job_push(Coroutine* coroutine);
    static Coroutine* job_pop();

    // creates a new coroutine
    static void coroutine_create(CoroutineFunc fn, void* args);
    // yield TLScheduler::cur_coroutine to TLScheduler::main_coroutine
    static void coroutine_yield();
    // swaps coroutine with TLScheduler::cur_coroutine. Does nothing except for swapping coroutine
    static void coroutine_swap(Coroutine* coroutine);
};

}