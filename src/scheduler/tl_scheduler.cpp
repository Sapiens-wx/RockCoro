#include "scheduler/tl_scheduler.h"

namespace rockcoro{

thread_local TLScheduler tl_scheduler;

TLScheduler::TLScheduler(){
    init();
}

void TLScheduler::init(){
}

TLScheduler& TLScheduler::get(){
    return tl_scheduler;
}

}