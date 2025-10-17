#pragma once

namespace rockcoro{

//if you want to use ctx_swap in a cpp file, add "INCLUDE_CTX_SWAP" on the top of the cpp file
#define INCLUDE_CTX_SWAP \
extern "C" {\
extern void ctx_swap(Context* from, Context* to) asm("ctx_swap");\
};

struct Coroutine;

typedef void (*CoroutineFunc)(void*);

struct Context{
    //the index of each register
    enum{
        R15=0,
        R14=1,
        R13=2,
        R12=3,
        R9=4,
        R8=5,
        RBP=6,
        RDI=7,
        RSI=8,
        RIP=9,
        RDX=10,
        RCX=11,
        RBX=12,
        RSP=13
    };
    // 64 bit
    // regs[0]: r15
    // regs[1]: r14
    // regs[2]: r13
    // regs[3]: r12
    // regs[4]: r9 
    // regs[5]: r8 
    // regs[6]: rbp
    // regs[7]: rdi
    // regs[8]: rsi
    // regs[9]: rip //return function addr
    // regs[10]: rdx
    // regs[11]: rcx
    // regs[12]: rbx
    // regs[13]: rsp
    void* regs[14];

    Context(Coroutine& coroutine);
};

}