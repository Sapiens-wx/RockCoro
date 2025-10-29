#include "scheduler/tl_scheduler.h"
#include "scheduler.h"
#include "coroutine/coroutine.h"
#include "timer/timewheel.h"

namespace rockcoro
{

    thread_local TLScheduler TLScheduler::inst;

    TLScheduler::TLScheduler()
        : pending_push(nullptr), pending_destroy(nullptr)
    {
        main_coroutine = new Coroutine(nullptr, nullptr);
        cur_coroutine = main_coroutine;
    }

    TLScheduler::~TLScheduler()
    {
        if (pending_destroy)
        {
            delete pending_destroy;
        }
        delete main_coroutine;
    }

    void TLScheduler::flush_pending_push()
    {
        if (pending_push)
        {
            Scheduler::inst.job_push(pending_push);
            pending_push = nullptr;
        }
    }

    void TLScheduler::flush_pending_destroy()
    {
        if (pending_destroy)
        {
            delete pending_destroy;
            pending_destroy = nullptr;
        }
    }

    void TLScheduler::flush_pending_add_event()
    {
        if (pending_add_event)
        {
            Timer::inst.add_event(pending_add_event, pending_add_event->timewheel_node.delayMS);
            pending_add_event = nullptr;
        }
    }

}
