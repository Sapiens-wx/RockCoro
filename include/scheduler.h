#pragma once
#include "basic_struct/data_structures.h"
#include <pthread.h>
#include <semaphore.h>

namespace rockcoro
{

#define SCHEDULER_NUM_WORKERS 2
    struct Coroutine;

    typedef void (*CoroutineFunc)(void *);

    struct Scheduler
    {
        static Scheduler inst;
        /// @brief the job queue
        LinkedList job_queue;
        pthread_spinlock_t spin_job_queue;
        sem_t sem_job_queue;
        // workers
        pthread_t workers[SCHEDULER_NUM_WORKERS];
        pthread_t timewheel_worker;
        // if Scheduler::destroy() is called, then running is set to false.
        // otherwise running is true.
        bool running = true;

        Scheduler();
        ~Scheduler();
        init();
        destroy();
        /// @brief gets the scheduler instance
        void job_push(Coroutine *coroutine);
        Coroutine *job_pop();

        // creates a new coroutine
        void coroutine_create(CoroutineFunc fn, void *args);
        // yield TLScheduler::cur_coroutine to TLScheduler::main_coroutine
        void coroutine_yield();
        // swaps coroutine with TLScheduler::cur_coroutine. Does nothing except for swapping coroutine
        void coroutine_swap(Coroutine *coroutine);
        // swaps coroutine with TLScheduler::cur_coroutine.
        // coroutine must be returning (i.e., the coroutine will not be run again)
        void coroutine_exit_swap(Coroutine *coroutine);
        // yields the coroutine and adds this coroutine to job_queue after [delayMS] ms
        void coroutine_sleep(int delayMS);
    };

}
