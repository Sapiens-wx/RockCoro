#include "context.h"
#include <memory.h>
#include "coroutine/coroutine.h"
#include "coroutine/stack.h"
#include "log.h"
#include "scheduler.h"
#include "scheduler/tl_scheduler.h"

namespace rockcoro {

// wrap the coroutine function with this handler function.
// when the coroutine returns, it will go back to this function, and it will handle the clean up stuff.
// we wrap the coroutine function instead of setting the return address of the coroutine function to this function because if we do, we do not have a stack to execute the handler function.
static void coroutine_entry_function(Coroutine *coroutine, void *args)
{
    if (coroutine->fn_) {
        coroutine->fn_(args);
    }
    // adds the coroutine to the pending_destroy
    TLScheduler &tl_scheduler = TLScheduler::inst;
    tl_scheduler.pending_destroy_ = coroutine;
    // return to the main coroutine (event loop)
    Scheduler::inst.coroutine_exit_swap(&tl_scheduler.main_coroutine_);
}

void CoroutineContext::init(Coroutine &coroutine)
{
    // pointer to stack base. leave three pointer space for:
    // ---stack base---
    //  return address
    //   first param (also for return address. consider this memory block as a union{first param; return address})
    //   second param
    // ---buffer end---
    char *sp = coroutine.stack_.stack_mem_ + coroutine.stack_.size_ - sizeof(void *) * 3;
    *(void **)(sp) = (void *)&coroutine_entry_function;    // return address
    *(void **)(sp + sizeof(void *)) = &coroutine;          // first param
    *(void **)(sp + sizeof(void *) * 2) = coroutine.args_; // second param
    // sets rbp and rsp to the bottom of the stack
    // leave sizeof(void*) bytes space for setting the return address in ctx_swap
    rsp_ = sp;
    rbp_ = sp;
}

} // namespace rockcoro
