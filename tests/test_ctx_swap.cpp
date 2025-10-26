#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>
#include "coroutine/coroutine.h"
#include "coroutine/context.h"
#include "scheduler.h"

using namespace rockcoro;

struct CoroParams
{
    Coroutine *self;
    Coroutine *originalCoro;
    int i;
};

static void coro1(void *args)
{
    CoroParams *cp = (CoroParams *)args;
    printf("this is coroutine 1, i=%d\n", cp->i);
    cp->i = 2;
}

TEST(ContextSwapTest, SingleFunctionTest)
{
    printf("on main routine\n");
    Coroutine mainCoro(nullptr, nullptr);
    CoroParams args;
    Coroutine coro(&coro1, &args);
	coro.ctx.init(coro);
    args.self = &coro;
    args.originalCoro = &mainCoro;
    args.i = 1;
	Scheduler::inst.coroutine_create(&coro1, &args);
	sleep(2);
    printf("main routine, i=%d\n", args.i);
}
