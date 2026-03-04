#include "scheduler.h"
#include "coroutine/context.h"
#include "coroutine/coroutine.h"
#include "log.h"
#include "memory/epoch_based_reclamation.h"
#include "timer/timewheel.h"
#include "tl_scheduler.h"

namespace rockcoro {

void *Scheduler::event_loop(void *)
{
    // if true, then means that the main loop yielded from a coroutine,
    // so we will push the job without post_sem, and then poping the job without wait_sem,
    // which is equivalent to post_self.
    bool yield_from_coroutine = false;
    while (Scheduler::inst.running_) {
        Coroutine *job = Scheduler::inst.job_pop(!yield_from_coroutine);
        if (job == nullptr) {
            continue;
        }
        Scheduler::inst.coroutine_swap(job);
        yield_from_coroutine = TLScheduler::inst.pending_push_ != nullptr;
        TLScheduler::inst.flush_pending_push();
        TLScheduler::inst.flush_pending_destroy();
        TLScheduler::inst.flush_pending_add_event();
    }
    return nullptr;
}

Scheduler::Scheduler()
{
}

void Scheduler::init()
{
    job_queue_.init();
    pthread_spin_init(&spin_job_queue_, 0);
    sem_init(&sem_job_queue_, 0, 0);
    for (int i = 0; i < SCHEDULER_NUM_WORKERS; ++i) {
        pthread_create(&workers_[i], nullptr, &event_loop, nullptr);
    }
    // timewheel eventloop
    pthread_create(&timewheel_worker_, nullptr, &TimerManager::event_loop, nullptr);
}

Scheduler::~Scheduler()
{
}

void Scheduler::destroy()
{
    running_ = false;
    // wake up all workers (in case they are waiting for the semaphore)
    for (int i = 0; i < SCHEDULER_NUM_WORKERS; ++i)
        sem_post(&sem_job_queue_);
    for (int i = 0; i < SCHEDULER_NUM_WORKERS; ++i)
        pthread_join(workers_[i], nullptr);
    pthread_join(timewheel_worker_, nullptr);
    pthread_spin_destroy(&spin_job_queue_);
    sem_destroy(&sem_job_queue_);
    job_queue_.destroy();
}

void Scheduler::job_push(Coroutine *coroutine, bool use_sem)
{
    job_queue_.push_back(coroutine);
    //logf("push %p %d\n", coroutine, (int)use_sem);
    if (use_sem)
        sem_post(&sem_job_queue_);
}
Coroutine *Scheduler::job_pop(bool use_sem)
{
    if (use_sem)
        sem_wait(&sem_job_queue_);
    Coroutine *value = job_queue_.pop_front();
    return value;
}
void Scheduler::coroutine_create(CoroutineFunc fn, void *args)
{
    Coroutine *coroutine = new Coroutine(fn, args);
    job_push(coroutine, true);
}
void Scheduler::coroutine_yield()
{
    TLScheduler &tl_scheduler = TLScheduler::inst;
    tl_scheduler.pending_push_ = tl_scheduler.cur_coroutine_;
    coroutine_swap(&tl_scheduler.main_coroutine_);
}
void Scheduler::coroutine_exit_swap(Coroutine *coroutine)
{
    TLScheduler &tl_scheduler = TLScheduler::inst;
    tl_scheduler.cur_coroutine_ = coroutine;
    ctx_exit_swap(coroutine);
}
void Scheduler::coroutine_swap(Coroutine *coroutine)
{
    TLScheduler &tl_scheduler = TLScheduler::inst;
    Coroutine *old_coroutine = tl_scheduler.cur_coroutine_;
    tl_scheduler.cur_coroutine_ = coroutine;
    if (coroutine->started_) {
        ctx_swap(old_coroutine, coroutine);
    } else // never started the coroutine. init the context
    {
        coroutine->started_ = true;
        coroutine->ctx_.init(*coroutine);
        ctx_entry_swap(old_coroutine, coroutine);
    }
}

void Scheduler::coroutine_sleep(int delayMS)
{
    TLScheduler &tl_scheduler = TLScheduler::inst;
    tl_scheduler.pending_add_event_ = tl_scheduler.cur_coroutine_;
    tl_scheduler.pending_add_event_->timewheel_node_.delayMS = delayMS;
    coroutine_swap(&tl_scheduler.main_coroutine_);
}

} // namespace rockcoro
