#include <memory.h>
#include "context.h"
#include "coroutine/coroutine.h"
#include "coroutine/stack.h"
#include "scheduler.h"
#include "scheduler/tl_scheduler.h"
#include "log.h"

namespace rockcoro
{

    // wrap the coroutine function with this handler function.
    // when the coroutine returns, it will go back to this function, and it will handle the clean up stuff.
    // we wrap the coroutine function instead of setting the return address of the coroutine function to this function because if we do, we do not have a stack to execute the handler function.
    static void coroutine_function_handler(Coroutine *coroutine, void *args)
    {
        if (coroutine->fn)
        {
            coroutine->fn(args);
        }
        // adds the coroutine to the pending_destroy
        TLScheduler &tl_scheduler = TLScheduler::inst;
        tl_scheduler.pending_destroy = coroutine;
        // return to the main coroutine (event loop)
        Scheduler::inst.coroutine_exit_swap(tl_scheduler.main_coroutine);
    }

    Context::Context(Coroutine &coroutine)
    {
        // if coroutine.fn==nullptr, then assumes this is the main coroutine.
        // So no need to initialize the register values
        if (coroutine.fn == nullptr)
            return;
        memset(regs, 0, sizeof(regs));
    }

    void Context::init(Coroutine &coroutine)
    {
        // pointer to stack base. leave three pointer space for:
        // ---stack base---
        //  return address
        //   first param (also for return address. consider this memory block as a union{first param; return address})
        //   second param
        // ---buffer end---
        char *sp = coroutine.stack->stack_mem->buffer + coroutine.stack->stack_mem->size - sizeof(void *) * 3;
        *(void **)(sp) = (void *)&coroutine_function_handler; // return address
        *(void **)(sp + sizeof(void *)) = &coroutine;         // first param
        *(void **)(sp + sizeof(void *) * 2) = coroutine.args; // second param
        // sets rbp and rsp to the bottom of the stack
        // leave sizeof(void*) bytes space for setting the return address in ctx_swap
        rsp = sp;
        rbp = sp;
    }

}
