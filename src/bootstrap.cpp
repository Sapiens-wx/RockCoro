#include "bootstrap.h"
#include "scheduler.h"
#include "timer/timewheel.h"

namespace rockcoro {

TimerManager TimerManager::inst;
Scheduler Scheduler::inst;

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
