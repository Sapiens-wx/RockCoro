#pragma once

namespace rockcoro
{

    struct CoroutineContext;
    struct Coroutine;

    extern "C" void ctx_swap(Coroutine *from, Coroutine *to) asm("ctx_swap");
    extern "C" void ctx_entry_swap(Coroutine *from, Coroutine *to) asm("ctx_entry_swap");
    extern "C" void ctx_exit_swap(Coroutine *to) asm("ctx_exit_swap");

    typedef void (*CoroutineFunc)(void *);

    struct CoroutineContext
    {
        // registers.
        // return address is stored in *rsp
        void *r12 = nullptr, *r13 = nullptr, *r14 = nullptr, *r15 = nullptr, *rbp = nullptr, *rsp = nullptr, *rbx = nullptr;

        CoroutineContext();
        // called when the coroutine is swapped for the first time
        void init(Coroutine &coroutine);
    };

}
