#include "scheduler/tl_scheduler.h"
#include "scheduler.h"
#include "coroutine/coroutine.h"

namespace rockcoro{

thread_local TLScheduler TLScheduler tl_scheduler;

TLScheduler::TLScheduler()
    : pending_push(nullptr), pending_destroy(nullptr){
    main_coroutine=new Coroutine(nullptr, nullptr);
	cur_coroutine=main_coroutine;
}

TLScheduler::~TLScheduler(){
    if(pending_destroy)
        delete pending_destroy;
    delete main_coroutine;
}

void TLScheduler::flush_pending_push(){
    if(pending_push){
        Scheduler::inst.job_push(pending_push);
        pending_push=nullptr;
    }
}

void TLScheduler::flush_pending_destroy(){
    if(pending_destroy){
        delete pending_destroy;
        pending_destroy=nullptr;
    }
}

}
