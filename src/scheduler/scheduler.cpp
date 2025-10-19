#include "scheduler.h"
#include "tl_scheduler.h"
#include "coroutine/context.h"
#include "coroutine/coroutine.h"
#include "log.h"

namespace rockcoro{

Scheduler Scheduler::inst;

static void* event_loop(void*){
    while(true){
        Coroutine* job=Scheduler::inst.job_pop();
		if(job==nullptr){
			logf("get job=nullptr\n");
			break;
		}
		while(job==nullptr){
			job=Scheduler::inst.job_pop();
		}
        Scheduler::inst.coroutine_swap(job);
        TLScheduler::inst.flush_pending_push();
        TLScheduler::inst.flush_pending_destroy();
    }
	return nullptr;
}

Scheduler::Scheduler(){
    pthread_spin_init(&spin_job_queue, 0);
    sem_init(&sem_job_queue, 0, 0);
    for(int i=0;i<SCHEDULER_NUM_WORKERS;++i){
        pthread_create(&workers[i], nullptr, &event_loop, nullptr);
		pthread_detach(workers[i]);
    }
}

Scheduler::~Scheduler(){
    pthread_spin_destroy(&spin_job_queue);
    sem_destroy(&sem_job_queue);
}

void Scheduler::job_push(Coroutine* coroutine){
    pthread_spin_lock(&spin_job_queue);
    job_queue.push_back(coroutine);
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
void Scheduler::coroutine_create(CoroutineFunc fn, void* args){
    Coroutine* coroutine=new Coroutine(fn, args);
    job_push(coroutine);
}
void Scheduler::coroutine_yield(){
    TLScheduler& tl_scheduler=TLScheduler::inst;
    tl_scheduler.pending_push=tl_scheduler.cur_coroutine;
    coroutine_swap(tl_scheduler.main_coroutine);
}
void Scheduler::coroutine_last_swap(Coroutine* coroutine){
    TLScheduler& tl_scheduler=TLScheduler::inst;
    Coroutine* old_coroutine=tl_scheduler.cur_coroutine;
    tl_scheduler.cur_coroutine=coroutine;
    ctx_last_swap(old_coroutine->ctx, coroutine->ctx);
}
void Scheduler::coroutine_swap(Coroutine* coroutine){
    TLScheduler& tl_scheduler=TLScheduler::inst;
    Coroutine* old_coroutine=tl_scheduler.cur_coroutine;
    tl_scheduler.cur_coroutine=coroutine;
    if(coroutine->started)
        ctx_swap(old_coroutine->ctx, coroutine->ctx);
    else{
		coroutine->started=true;
        ctx_first_swap(old_coroutine->ctx, coroutine->ctx);
	}
}

}
