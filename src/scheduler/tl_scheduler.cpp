#include "scheduler/tl_scheduler.h"
#include "coroutine/coroutine.h"
#include "scheduler.h"
#include "timer/timewheel.h"

namespace rockcoro {

thread_local TLScheduler TLScheduler::inst;

TLScheduler::TLScheduler()
{
    main_coroutine_.started_ = true;
    cur_coroutine_ = &main_coroutine_;
}

TLScheduler::~TLScheduler()
{
    if (pending_destroy_) {
        delete pending_destroy_;
    }
}

void TLScheduler::flush_pending_push()
{
    if (pending_push_) {
        Scheduler::inst.job_push(pending_push_, false);
        pending_push_ = nullptr;
    }
}

void TLScheduler::flush_pending_destroy()
{
    if (pending_destroy_) {
        delete pending_destroy_;
        pending_destroy_ = nullptr;
    }
}

void TLScheduler::flush_pending_add_event()
{
    if (pending_add_event_) {
        TimerManager::inst.add_event(pending_add_event_,
                                     pending_add_event_->timewheel_node_.delayMS);
        pending_add_event_ = nullptr;
    }
}

} // namespace rockcoro
