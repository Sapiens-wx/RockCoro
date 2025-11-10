#pragma once

namespace rockcoro {

struct Coroutine;

struct TLScheduler {
    static thread_local TLScheduler inst;
    // current coroutine this thread is running on
    Coroutine *cur_coroutine = nullptr;
    // the event loop.
    Coroutine *main_coroutine = nullptr;
    // the coroutine that called yield. A coroutine has to be pushed to job_queue after it returns to the main coroutine.
    Coroutine *pending_push = nullptr;
    // the coroutine that has terminated. Destroy the coroutine instance after the coroutine returns.
    Coroutine *pending_destroy = nullptr;
    // if Scheduler::coroutine_sleep is called, call TimerManager::add_event after the coroutine yields
    Coroutine *pending_add_event = nullptr;

    TLScheduler();
    ~TLScheduler();
    // push pending_push to the job queue if pending_push!=null and set it to null
    void flush_pending_push();
    // destroy pending_destroy if pending_destroy!=null and set it to null
    void flush_pending_destroy();
    // call TimerManager::add_event(pending_add_event) if pending_add_event!=null and set it to null
    void flush_pending_add_event();
};

} // namespace rockcoro