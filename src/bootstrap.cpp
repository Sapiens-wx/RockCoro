#include "bootstrap.h"
#include "memory/epoch_based_reclamation.h"
#include "memory/ts_linked_list.h"
#include "scheduler.h"
#include "thread_info.h"
#include "timer/timewheel.h"

namespace rockcoro {

EpochBasedReclamation EpochBasedReclamation::inst;
Logger Logger::inst;
TSLinkedListNodeAllocator TSLinkedListNodeAllocator::inst;
thread_local ThreadInfo ThreadInfo::inst;
TimerManager TimerManager::inst;
Scheduler Scheduler::inst;

void init()
{
    TSLinkedListNodeAllocator::inst.init();
    EpochBasedReclamation::inst.init();
    TimerManager::inst.init();
    Scheduler::inst.init();

    EpochBasedReclamation::inst.init_thread_epoch();
}

void destroy()
{
    Scheduler::inst.destroy();
    TimerManager::inst.destroy();
    EpochBasedReclamation::inst.destroy();
    TSLinkedListNodeAllocator::inst.destroy();
}

} // namespace rockcoro
