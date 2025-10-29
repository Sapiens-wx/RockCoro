#pragma once

namespace rockcoro
{

    struct Context;
    struct Coroutine;

    extern "C" void ctx_swap(Coroutine *from, Coroutine *to) asm("ctx_swap");
    extern "C" void ctx_entry_swap(Coroutine *from, Coroutine *to) asm("ctx_entry_swap");
    extern "C" void ctx_exit_swap(Coroutine *to) asm("ctx_exit_swap");

    typedef void (*CoroutineFunc)(void *);

    struct Context
    {
        // registers.
        // return address is stored in *rsp
        void *r15, *r14, *r13, *r12, *rbp, *rsp, *rbx;

        Context();
        // called when the coroutine is swapped for the first time
        void init(Coroutine &coroutine);
    };

}
