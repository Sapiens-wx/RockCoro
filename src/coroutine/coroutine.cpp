#include "coroutine/coroutine.h"
#include "coroutine/stack.h"
#include "coroutine/context.h"

namespace rockcoro{

Coroutine::Coroutine(CoroutineFunc fn, void* args)
    :fn(fn), args(args){
    stack=new Stack();
    ctx=new Context(*this);
}

Coroutine::~Coroutine(){
    delete stack;
    delete ctx;
}

}