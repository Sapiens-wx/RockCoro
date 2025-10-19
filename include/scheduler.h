#pragma once
#include "basic_struct/data_structures.h"
#include <pthread.h>
#include <semaphore.h>

namespace rockcoro{

#define SCHEDULER_NUM_WORKERS 2
struct Coroutine;

typedef void (*CoroutineFunc)(void*);

struct Scheduler{
    static Scheduler inst;
    /// @brief the job queue
    LinkedList job_queue;
    pthread_spinlock_t spin_job_queue;
    sem_t sem_job_queue;
    // workers
    pthread_t workers[SCHEDULER_NUM_WORKERS];

    Scheduler();
    ~Scheduler();
    /// @brief gets the scheduler instance
    void job_push(Coroutine* coroutine);
    Coroutine* job_pop();

    // creates a new coroutine
    void coroutine_create(CoroutineFunc fn, void* args);
    // yield TLScheduler::cur_coroutine to TLScheduler::main_coroutine
    void coroutine_yield();
    // swaps coroutine with TLScheduler::cur_coroutine. Does nothing except for swapping coroutine
    void coroutine_swap(Coroutine* coroutine);
    // swaps coroutine with TLScheduler::cur_coroutine.
    // coroutine must be returning (i.e., the coroutine will not be run again)
    void coroutine_last_swap(Coroutine* coroutine);
};

}
