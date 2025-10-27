#pragma once

namespace rockcoro{

struct Context;
struct Coroutine;

extern "C" void ctx_swap(Context* from, Context* to) asm("ctx_swap");
extern "C" void ctx_entry_swap(Context* from, Context* to) asm("ctx_entry_swap");
extern "C" void ctx_exit_swap(Context* to) asm("ctx_exit_swap");

typedef void (*CoroutineFunc)(void*);

struct Context{
    // registers.
    // return address is stored in *rsp
    void* r15, *r14, *r13, *r12, *rbp, *rsp, *rbx;

    Context(Coroutine& coroutine);
};

}
