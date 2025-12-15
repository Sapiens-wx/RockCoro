#include "bootstrap.h"
#include "scheduler.h"
#include "thread_info.h"
#include "timer/timewheel.h"

namespace rockcoro {

TimerManager TimerManager::inst;
Scheduler Scheduler::inst;
thread_local ThreadInfo ThreadInfo::inst;

void init()
{
    TimerManager::inst.init();
    Scheduler::inst.init();
}

void destroy()
{
    Scheduler::inst.destroy();
    TimerManager::inst.destroy();
}

} // namespace rockcoro
