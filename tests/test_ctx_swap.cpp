#include <gtest/gtest.h>
#include <stdio.h>
#include "coroutine/coroutine.h"
#include "coroutine/context.h"

using namespace rockcoro;

struct CoroParams{
    Coroutine* self;
    Coroutine* originalCoro;
	int i;
};

static void coro1(void* args){
    CoroParams* cp=(CoroParams*)args;
    printf("this is coroutine 1, i=%d\n", cp->i);
	cp->i=2;
    ctx_swap(cp->self->ctx, cp->originalCoro->ctx);
}

TEST(ContextSwapTest, SingleFunctionTest){
    printf("on main routine\n");
    Coroutine mainCoro(nullptr, nullptr);
    CoroParams args;
    Coroutine coro(&coro1, &args);
    args.self=&coro;
    args.originalCoro=&mainCoro;
	args.i=1;
    ctx_swap(mainCoro.ctx, coro.ctx);
	printf("main routine, i=%d\n", args.i);
}
