#pragma once

namespace rockcoro{

struct Stack;
struct Context;

typedef void (*CoroutineFunc)(void*);

struct Coroutine{
    Stack* stack;
    Context* ctx;

    // the function that this coroutine runs on.<br>
    // If cn==nullptr, assumes that this is the main coroutine, 
    // and its stack will not be allocated and its context will 
    // be created but not initialized
    CoroutineFunc fn;
    // args the parameters of fn
    void* args;

    // @param fn the function that this coroutine runs on
    // @param args the parameters of fn
    Coroutine(CoroutineFunc fn, void* args);
    ~Coroutine();
};

}
