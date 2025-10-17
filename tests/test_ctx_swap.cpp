#include <gtest/gtest.h>
#include <stdio.h>
#include "coroutine/coroutine.h"
#include "coroutine/context.h"

INCLUDE_CTX_SWAP

struct CoroParams{
    Coroutine* self;
    Coroutine* originalCoro;
};

static void coro1(void* args){
    printf("this is coroutine 1\n");
    CoroParams* cp=(CoroParams*)args;
    ctx_swap(cp->self, cp->originalCoro);
}

TEST(ContextSwapTest, SingleFunctionTest){
    printf("on main routine\n");
    Coroutine mainCoro(nullptr, nullptr);
    CoroParams args;
    Coroutine coro(&coro1, &args);
    args.self=&coro;
    args.originalCoro=&mainCoro;
    ctx_swap(mainCoro.ctx, coro.ctx);
}