#pragma once

namespace rockcoro{

struct Coroutine;

struct TLScheduler{
    Coroutine* cur_coroutine;

    TLScheduler();
    void init();
    /// @brief gets the thread-local TLScheduler instance
    static TLScheduler& get();
};

}