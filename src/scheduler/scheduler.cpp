#include "scheduler.h"

namespace rockcoro{

Scheduler scheduler;

Scheduler::Scheduler(){
    init();
}

void Scheduler::init(){
}

Scheduler& Scheduler::get(){
    return scheduler;
}

}