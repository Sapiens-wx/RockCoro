#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <stdio.h>
#include <chrono>
#include "log.h"
#include "scheduler.h"

using namespace rockcoro;
using namespace std::chrono;

struct ParamTestSleep
{
    int sleep_ms;
    int actual_sleep_ms;
    bool is_finished;
};

void
test_sleep(void *args)
{
    ParamTestSleep *param = (ParamTestSleep *)args;

    // 记录开始时间
    auto start = steady_clock::now();

    // 调用待测函数
    Scheduler::inst.coroutine_sleep(param->sleep_ms);

	// 记录结束时间
	auto end = steady_clock::now();

    // 计算实际耗时
    auto elapsed = duration_cast<milliseconds>(end - start).count();
    param->actual_sleep_ms = (int)elapsed;
    param->is_finished = true;
}

TEST(TimeWheelTest, SleepTest)
{
#define SLEEP_MS_FUNC(t) (t * 20)
    constexpr const int num_coroutines = 100;

    int sleep_ms = 0;
    ParamTestSleep params[num_coroutines];
    for (int i = 0; i < num_coroutines; ++i)
    {
        sleep_ms = SLEEP_MS_FUNC(i);
        params[i].sleep_ms = sleep_ms;
        params[i].is_finished = false;
        Scheduler::inst.coroutine_create(&test_sleep, &params[i]);
    }

    bool finished = false;
    while (!finished)
    {
        sleep(1);
        finished = true;
        for (int i = 0; i < num_coroutines; ++i)
        {
            finished = finished && params[i].is_finished;
            if (finished == false)
            {
                break;
            }
        }
        printf("waiting for coroutines to completed\n");
    }
    for (int i = 0; i < num_coroutines; ++i)
    {
        EXPECT_EQ(params[i].sleep_ms, params[i].actual_sleep_ms);
    }
}
