#pragma once
#include "basic_struct/data_structures.h"
#include "coroutine/context.h"
#include "coroutine/stack.h"
#include "timer/timewheel.h"

namespace rockcoro {

typedef void (*CoroutineFunc)(void *);

struct Coroutine {
    CoroutineContext ctx;
    Stack stack;

    // the function that this coroutine runs on.<br>
    // If cn==nullptr, assumes that this is the main coroutine,
    // and its stack will not be allocated and its context will
    // be created but not initialized
    CoroutineFunc fn = nullptr;
    // args the parameters of fn
    void *args = nullptr;

    // the node used when this coroutine is pushed or poped from a LinkedList
    TLLinkedListNode node;
    // the node used when this coroutine is added to the time wheel
    TimeWheelLinkedListNode timewheel_node;

    // has the coroutine started?
    // if not, ctx_first_swap will be used instead of ctx_swap.
    bool started = false;

    // @param fn the function that this coroutine runs on
    // @param args the parameters of fn
    Coroutine(CoroutineFunc fn, void *args);
};

} // namespace rockcoro
