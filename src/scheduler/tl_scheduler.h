#pragma once

namespace rockcoro{

struct Coroutine;

struct TLScheduler{
    static thread_local TLScheduler inst;
    // current coroutine this thread is running on
    Coroutine* cur_coroutine;
    // the event loop.
    Coroutine* main_coroutine;
    // the coroutine that called yield. A coroutine has to be pushed to job_queue after it returns to the main coroutine.
    Coroutine* pending_push;
    // the coroutine that has terminated. Destroy the coroutine instance after the coroutine returns.
    Coroutine* pending_destroy;

    TLScheduler();
    ~TLScheduler();
    // push pending_push to the job queue if pending_push!=null and set it to null
    void flush_pending_push();
    // destroy pending_destroy if pending_destroy!=null and set it to null
    void flush_pending_destroy();
};

}