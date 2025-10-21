#include "scheduler.h"
#include "tl_scheduler.h"
#include "coroutine/context.h"
#include "coroutine/coroutine.h"

#define EVENT_LOOP_SPIN_COUNT 100

namespace rockcoro{

Scheduler scheduler;

static void event_loop(){
    while(true){
        Coroutine* job=nullptr;
        for(int i=0;i<EVENT_LOOP_SPIN_COUNT&&job==nullptr;++i){
            job=scheduler.job_pop();
        }
        if(job==nullptr){
            pthread_mutex_lock(&scheduler.mutex_job_queue);
            pthread_cond_wait(&scheduler.cond_job_queue, &scheduler.mutex_job_queue);
            pthread_mutex_unlock(&scheduler.mutex_job_queue);
            continue;
        }
        Scheduler::coroutine_swap(job);
        TLScheduler::get().flush_pending_push();
        TLScheduler::get().flush_pending_destroy();
    }
}

Scheduler::Scheduler(){
    for(int i=0;i<SCHEDULER_NUM_WORKERS;++i){
        pthread_create(&workers[i], nullptr, &event_loop, nullptr);
    }
    pthread_mutex_init(&mutex_job_queue, nullptr);
    pthread_cond_init(&cond_job_queue, nullptr);
}

Scheduler::~Scheduler(){
    pthread_mutex_destroy(&mutex_job_queue);
    pthread_cond_destroy(&cond_job_queue);
}

Scheduler& Scheduler::get(){
    return scheduler;
}

void Scheduler::job_push(Coroutine* coroutine){
    pthread_mutex_lock(&scheduler.mutex_job_queue);
    scheduler.job_queue.push(coroutine);
    pthread_cond_signal(&scheduler.cond_job_queue, &scheduler.mutex_job_queue);
    pthread_mutex_unlock(&scheduler.mutex_job_queue);
}
Coroutine* Scheduler::job_pop(){
    pthread_mutex_lock(&scheduler.mutex_job_queue);
    Coroutine* ret=scheduler.job_queue.pop();
    pthread_mutex_unlock(&scheduler.mutex_job_queue);
    return ret;
}
void Scheduler::coroutine_create(CoroutineFunc fn, void* args){
    Coroutine* coroutine=new Coroutine(fn, args);
    job_push(coroutine);
}
void Scheduler::coroutine_yield(){
    TLScheduler& tl_scheduler=TLScheduler::get();
    tl_scheduler.pending_push=tl_scheduler.cur_coroutine;
    coroutine_swap(tl_scheduler.main_coroutine);
}
void Scheduler::coroutine_swap(Coroutine* coroutine){
    TLScheduler& tl_scheduler=TLScheduler::get();
    Coroutine* old_coroutine=tl_scheduler.cur_coroutine;
    tl_scheduler.cur_coroutine=coroutine;
    ctx_swap(old_coroutine->ctx, coroutine->ctx);
}

}