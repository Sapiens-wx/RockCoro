#include <memory.h>
#include "context.h"
#include "coroutine/coroutine.h"
#include "coroutine/stack.h"
#include "scheduler.h"
#include "scheduler/tl_scheduler.h"
#include "log.h"

namespace rockcoro{

// wrap the coroutine function with this handler function.
// when the coroutine returns, it will go back to this function, and it will handle the clean up stuff.
// we wrap the coroutine function instead of setting the return address of the coroutine function to this function because if we do, we do not have a stack to execute the handler function.
static void coroutine_function_handler(Coroutine* coroutine, void* args){
    if(coroutine->fn){
        coroutine->fn(args);
    }
    // adds the coroutine to the pending_destroy
    TLScheduler& tl_scheduler=TLScheduler::inst;
    tl_scheduler.pending_destroy=coroutine;
    // return to the main coroutine (event loop)
    Scheduler::inst.coroutine_swap(tl_scheduler.main_coroutine);
}

Context::Context(Coroutine& coroutine){
    // if coroutine.fn==nullptr, then assumes this is the main coroutine.
    // So no need to initialize the register values
    if(coroutine.fn==nullptr)
        return;
    memset(regs, 0, sizeof(regs));
    // set RIP to coroutine_function_handler
    regs[RIP]=(void*)&coroutine_function_handler;
    // set the first param to &coroutine
    regs[RDI]=&coroutine;
    // set the second param to args
    regs[RSI]=coroutine.args;
    // sets rbp and rsp to the bottom of the stack
    // leave sizeof(void*) bytes space for setting the return address in ctx_swap
    regs[RBP]=coroutine.stack->stack_mem->buffer+coroutine.stack->stack_mem->size-sizeof(void*);
    regs[RSP]=regs[RBP];
}

}
