#pragma once

namespace rockcoro{

struct Context;
struct Coroutine;

extern "C" void ctx_swap(Context* from, Context* to) asm("ctx_swap");
extern "C" void ctx_first_swap(Context* from, Context* to) asm("ctx_first_swap");
extern "C" void ctx_last_swap(Context* from, Context* to) asm("ctx_last_swap");

typedef void (*CoroutineFunc)(void*);

struct Context{
    //the index of each register
    enum{
        R15=0,
        R14=1,
        R13=2,
        R12=3,
        RBP=4,
        RIP=5,
        RBX=6,
        RSP=7
    };
    // 64 bit
    // regs[0]: r15
    // regs[1]: r14
    // regs[2]: r13
    // regs[3]: r12
    // regs[4]: rbp
    // regs[5]: rip //return function addr
    // regs[6]: rbx
    // regs[7]: rsp
    void* regs[8];

    Context(Coroutine& coroutine);
};

}
