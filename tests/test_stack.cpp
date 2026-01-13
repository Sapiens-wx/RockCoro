#include <gtest/gtest.h>
#include <stdio.h>
#include <thread>
#include "log.h"
#include "scheduler.h"


using namespace rockcoro;

struct StackParams {
    int value;
    bool completed = false;
    StackParams()
    {
    }
};

void coroutine_func_no_overflow(void *args)
{
    StackParams *param = (StackParams *)args;
    char buf[STACK_SIZE - sizeof(void *) * 100];
    buf[0] = 0;
    int i = buf[0];
    param->value = i;
}

void coroutine_func_overflow(void *args)
{
    StackParams *param = (StackParams *)args;
    char buf[STACK_SIZE + 1];
    buf[0] = 0;
    int i = buf[0];
    param->value = i;
}

TEST(StackTest, StackOverflowTest)
{
    logf("if the program throws a segmentation fault exception on func coroutine_func_overflow, "
         "then the test is passed.");
    StackParams param_no_overflow, param_overflow;
    param_no_overflow.value = -1;
    param_overflow.value = -1;
    Scheduler::inst.coroutine_create(&coroutine_func_no_overflow, &param_no_overflow);
    Scheduler::inst.coroutine_create(&coroutine_func_overflow, &param_overflow);
    bool completed = false;
    while (!completed) {
        completed &= param_no_overflow.completed;
        completed &= param_overflow.completed;
        printf("waiting...");
        sleep(1);
    }
    EXPECT_EQ(param_no_overflow.value, 0);
    EXPECT_NE(param_overflow.value, 0);
}
