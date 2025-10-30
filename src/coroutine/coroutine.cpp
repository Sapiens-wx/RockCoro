#include "coroutine/coroutine.h"
#include "coroutine/stack.h"
#include "coroutine/context.h"

namespace rockcoro
{

    Coroutine::Coroutine(CoroutineFunc fn, void *args)
        : fn(fn), args(args), node(this), timewheel_node(this)
    {
    }

}
