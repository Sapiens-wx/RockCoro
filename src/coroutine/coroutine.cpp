#include "coroutine/coroutine.h"
#include "coroutine/context.h"
#include "coroutine/stack.h"


namespace rockcoro {

Coroutine::Coroutine(CoroutineFunc fn, void *args)
    : fn(fn)
    , args(args)
    , node(this)
    , timewheel_node(this)
{
}

} // namespace rockcoro
