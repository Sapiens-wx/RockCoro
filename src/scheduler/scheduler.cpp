#include "scheduler.h"
#include "tl_scheduler.h"
#include "coroutine/context.h"
#include "coroutine/coroutine.h"
#include "log.h"

namespace rockcoro{

Scheduler Scheduler::inst;

static void* event_loop(void*){
    while(true){
        Coroutine* job=inst.job_pop();
        if(job==nullptr){
            logf("job should not be null");
            break;
        }
        inst.coroutine_swap(job);
        TLScheduler::inst.flush_pending_push();
        TLScheduler::inst.flush_pending_destroy();
    }
}

Scheduler::Scheduler(){
    pthread_spinlock_init(&spin_job_queue, nullptr);
    sem_init(&sem_job_queue, 0, 0);
    for(int i=0;i<SCHEDULER_NUM_WORKERS;++i){
        pthread_create(&workers[i], nullptr, &event_loop, nullptr);
		pthread_detach(workers[i]);
    }
}

Scheduler::~Scheduler(){
    pthread_spinlock_destroy(&spin_job_queue);
    sem_destroy(&sem_job_queue);
    pthread_cond_destroy(&cond_job_queue);
}

void Scheduler::job_push(Coroutine* coroutine){
    pthread_spin_lock(&spin_job_queue);
    job_queue.push_back(coroutine);
    pthread_cond_signal(&cond_job_queue);
    pthread_spin_unlock(&spin_job_queue);
    sem_post(&sem_job_queue);
}
Coroutine* Scheduler::job_pop(){
    sem_wait(&sem_job_queue);
    pthread_spin_lock(&spin_job_queue);
    Coroutine* ret=job_queue.pop_front();
    pthread_spin_unlock(&spin_job_queue);
    return ret;
}
Coroutine* Scheduler::job_pop_no_lock(){
    return job_queue.pop_front();
}
void Scheduler::coroutine_create(CoroutineFunc fn, void* args){
    Coroutine* coroutine=new Coroutine(fn, args);
    job_push(coroutine);
}
void Scheduler::coroutine_yield(){
    TLScheduler& tl_scheduler=TLScheduler::inst;
    tl_scheduler.pending_push=tl_scheduler.cur_coroutine;
    coroutine_swap(tl_scheduler.main_coroutine);
}
void Scheduler::coroutine_swap(Coroutine* coroutine){
    TLScheduler& tl_scheduler=TLScheduler::inst;
    Coroutine* old_coroutine=tl_scheduler.cur_coroutine;
    tl_scheduler.cur_coroutine=coroutine;
    ctx_swap(old_coroutine->ctx, coroutine->ctx);
}

}
