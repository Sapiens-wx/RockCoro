#pragma once
#include "basic_struct/data_structures.h"

namespace rockcoro{

#define SCHEDULER_NUM_WORKERS 10
struct Coroutine;

struct Scheduler{
    /// @brief constructor. It uses init() to initialize the scheduler
    Scheduler();
    /// @brief 
    void init();
    /// @brief gets the scheduler instance
    static Scheduler& get();

    /// @brief an array of pointers to every thread's job dequeue. This array it self is not thread-safe.
    CLDeque<Coroutine*>* job_deques[SCHEDULER_NUM_WORKERS];
};

}