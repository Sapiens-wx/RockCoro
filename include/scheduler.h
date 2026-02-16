#pragma once
#include <pthread.h>
#include <semaphore.h>
#include "config.h"
#include "memory/ts_linked_list.h"

namespace rockcoro {

// number of workers to execute the coroutines
constexpr int SCHEDULER_NUM_WORKERS = 10;

struct Coroutine;
using CoroutineFunc = void (*)(void *);

struct Scheduler {
public:
    static Scheduler inst;

private:
    /// @brief the job queue
    TSLinkedList job_queue_;
    pthread_spinlock_t spin_job_queue_;
    sem_t sem_job_queue_;
    // workers
    pthread_t workers_[SCHEDULER_NUM_WORKERS];
    pthread_t timewheel_worker_;

public:
    // if Scheduler::destroy() is called, then running is set to false.
    // otherwise running is true.
    bool running_ = true;

public:
    Scheduler();
    ~Scheduler();
    void init();
    void destroy();
    /// @brief pushes a job, and post_sem if use_sem==true
    void job_push(Coroutine *coroutine, bool use_sem);
    /// @brief (wait_sem if use_sem==true,) pops a job
    Coroutine *job_pop(bool use_sem);

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

private:
    static void *event_loop(void *);
};

} // namespace rockcoro
