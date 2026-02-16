#pragma once
#include "coroutine/context.h"
#include "coroutine/stack.h"
#include "timer/timewheel.h"

namespace rockcoro {

typedef void (*CoroutineFunc)(void *);

struct Coroutine {
    CoroutineContext ctx_;
    Stack stack_;

    // the function that this coroutine runs on.<br>
    // If cn==nullptr, assumes that this is the main coroutine,
    // and its stack will not be allocated and its context will
    // be created but not initialized
    CoroutineFunc fn_ = nullptr;
    // args the parameters of fn
    void *args_ = nullptr;

    // the node used when this coroutine is added to the time wheel
    TimeWheelLinkedListNode timewheel_node_;

    // has the coroutine started?
    // if not, ctx_first_swap will be used instead of ctx_swap.
    bool started_ = false;

    // @param fn the function that this coroutine runs on
    // @param args the parameters of fn
    Coroutine(CoroutineFunc fn, void *args);
};

} // namespace rockcoro
