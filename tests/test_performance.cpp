#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "coroutine/coroutine.h"
#include "scheduler.h"
#include "scheduler/tl_scheduler.h"

namespace {
using namespace rockcoro;
using Clock = std::chrono::steady_clock;
constexpr int kSamples = 10000;
constexpr int kWarmup = 1000;

double Nanoseconds(Clock::duration duration)
{
    return std::chrono::duration<double, std::nano>(duration).count();
}

void Report(const char *name, double value, const char *unit)
{
    EXPECT_GT(value, 0.0);
    std::printf("[ PERFORMANCE ] %s: %.3f %s\n", name, value, unit);
    testing::Test::RecordProperty(name, std::to_string(value));
}

void Complete(void *arg)
{
    static_cast<std::atomic<int> *>(arg)->fetch_add(1, std::memory_order_acq_rel);
}

void WaitFor(std::atomic<int> &completed, int count)
{
    // The CTest timeout bounds the run. Never release callback arguments early.
    while (completed.load(std::memory_order_acquire) != count)
        std::this_thread::yield();
}

void WarmupScheduler()
{
    std::atomic<int> completed{0};
    for (int i = 0; i < kWarmup; ++i)
        Scheduler::inst.coroutine_create(Complete, &completed);
    WaitFor(completed, kWarmup);
}

TEST(PerformanceTest, CoroutineCreationLatency)
{
    WarmupScheduler();
    std::atomic<int> completed{0};
    std::vector<double> samples(kSamples);
    for (double &sample : samples) {
        const auto start = Clock::now();
        Scheduler::inst.coroutine_create(Complete, &completed);
        sample = Nanoseconds(Clock::now() - start);
    }
    WaitFor(completed, kSamples);
    EXPECT_EQ(completed.load(), kSamples);
    // Includes allocation, enqueue and notification with live worker threads.
    Report("creation_mean_ns", std::accumulate(samples.begin(), samples.end(), 0.0) / kSamples,
           "ns/create");
}

void Empty(void *) {}

TEST(PerformanceTest, CoroutineDestructionLatency)
{
    double total = 0;
    for (int i = 0; i < kWarmup + kSamples; ++i) {
        auto *coroutine = new Coroutine(Empty, nullptr);
        // Allocation is outside the timed region; delete includes stack unmapping.
        const auto start = Clock::now();
        delete coroutine;
        const auto elapsed = Clock::now() - start;
        if (i >= kWarmup)
            total += Nanoseconds(elapsed);
    }
    Report("destruction_mean_ns", total / kSamples, "ns/delete");
}

struct SwapState {
    Coroutine *main;
    bool stop = false;
    int visits = 0;
};

void PingPong(void *arg)
{
    auto &state = *static_cast<SwapState *>(arg);
    while (!state.stop) {
        ++state.visits;
        Scheduler::inst.coroutine_swap(state.main);
    }
}

TEST(PerformanceTest, ContextSwitchLatency)
{
    auto &tls = TLScheduler::inst;
    SwapState state{&tls.main_coroutine_};
    auto *coroutine = new Coroutine(PingPong, &state);
    for (int i = 0; i < kWarmup; ++i)
        Scheduler::inst.coroutine_swap(coroutine);
    constexpr int rounds = 1000000;
    const auto start = Clock::now();
    for (int i = 0; i < rounds; ++i)
        Scheduler::inst.coroutine_swap(coroutine);
    const auto elapsed = Clock::now() - start;
    EXPECT_EQ(state.visits, kWarmup + rounds);
    // Each round contains two direct swaps. Excludes first entry and final exit;
    // includes the small loop overhead, without per-switch clock reads.
    state.stop = true;
    Scheduler::inst.coroutine_swap(coroutine);
    EXPECT_EQ(tls.cur_coroutine_, &tls.main_coroutine_);
    EXPECT_EQ(tls.pending_destroy_, coroutine);
    tls.flush_pending_destroy();
    Report("context_switch_mean_ns", Nanoseconds(elapsed) / (2.0 * rounds), "ns/switch");
}

TEST(PerformanceTest, SchedulingThroughput)
{
    WarmupScheduler();
    constexpr int jobs = 100000;
    std::atomic<int> completed{0};
    const auto start = Clock::now();
    for (int i = 0; i < jobs; ++i)
        Scheduler::inst.coroutine_create(Complete, &completed);
    WaitFor(completed, jobs);
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    EXPECT_EQ(completed.load(), jobs);
    // One producer, SCHEDULER_NUM_WORKERS consumers, one execution per job.
    // End-to-end rate includes submission and completion-counter overhead.
    Report("scheduling_throughput_jobs_per_second", jobs / seconds, "jobs/s");
}

struct LatencySample {
    Clock::time_point submitted;
    double elapsed_ns = 0;
    std::atomic<int> *completed;
};

void RecordStart(void *arg)
{
    const auto entered = Clock::now();
    auto &sample = *static_cast<LatencySample *>(arg);
    sample.elapsed_ns = Nanoseconds(entered - sample.submitted);
    sample.completed->fetch_add(1, std::memory_order_acq_rel);
}

TEST(PerformanceTest, SchedulingLatencyP99)
{
    WarmupScheduler();
    std::atomic<int> completed{0};
    std::vector<LatencySample> samples(kSamples);
    for (auto &sample : samples) {
        sample.completed = &completed;
        sample.submitted = Clock::now();
        Scheduler::inst.coroutine_create(RecordStart, &sample);
    }
    WaitFor(completed, kSamples);
    std::vector<double> latencies;
    latencies.reserve(kSamples);
    for (const auto &sample : samples) {
        EXPECT_GE(sample.elapsed_ns, 0.0);
        latencies.push_back(sample.elapsed_ns);
    }
    std::sort(latencies.begin(), latencies.end());
    // Nearest-rank p99 of a burst from one producer, including creation time.
    Report("scheduling_latency_p99_ns", latencies[(99 * kSamples + 99) / 100 - 1], "ns");
}
} // namespace
