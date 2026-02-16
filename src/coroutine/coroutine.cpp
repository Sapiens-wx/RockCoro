#include "coroutine/coroutine.h"
#include "coroutine/context.h"
#include "coroutine/stack.h"

namespace rockcoro {

Coroutine::Coroutine(CoroutineFunc fn, void *args)
    : fn_(fn)
    , args_(args)
    , timewheel_node_(this)
{
}

} // namespace rockcoro
