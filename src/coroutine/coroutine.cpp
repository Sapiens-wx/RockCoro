#include "coroutine/coroutine.h"
#include "coroutine/stack.h"
#include "coroutine/context.h"

namespace rockcoro{

Coroutine::Coroutine(CoroutineFunc fn, void* args)
    :stack(nullptr), fn(fn), args(args), node(this), started(false){
    if(fn)
        stack=new Stack();
    ctx=new Context(*this);
}

Coroutine::~Coroutine(){
    if(stack)
        delete stack;
    delete ctx;
}

}
