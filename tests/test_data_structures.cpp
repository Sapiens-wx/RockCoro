#include <assert.h>
#include <atomic>
#include <csignal>
#include <gtest/gtest.h>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "basic_struct/data_structures.h"
#include "config.h"
#include "coroutine/coroutine.h"
#include "log.h"
#include "memory/epoch_based_reclamation.h"
#include "scheduler.h"

using namespace rockcoro;

constexpr int PRODUCER_COUNT = 16;
constexpr int CONSUMER_COUNT = 16;
constexpr int ITEMS_PER_PRODUCER = 1000000;

// if assertion fails: must have enough ThreadEpoch for the testing worker threads
static_assert(PRODUCER_COUNT + CONSUMER_COUNT < EBR_MAX_THREADS - SCHEDULER_NUM_WORKERS - 1);

/*
TEST(TSLinkedListTest, PushPopTest)
{
    TSLinkedList queue;
    queue.init();

    constexpr int TOTAL_ITEMS = PRODUCER_COUNT * ITEMS_PER_PRODUCER;

    // 每个元素是一个唯一整数
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    // 记录 pop 结果（用于查重）
    std::vector<std::atomic<int>> seen(TOTAL_ITEMS + 1);
    for (auto &v : seen)
        v.store(0, std::memory_order_relaxed);

    // producer
    auto producer = [&]() {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            int id = produced.fetch_add(1, std::memory_order_relaxed) + 1;
            queue.push_back(reinterpret_cast<void *>(static_cast<intptr_t>(id)));
        }
    };

    // consumer
    auto consumer = [&]() {
        int pop_count = 0;
        while (consumed.load(std::memory_order_acquire) < TOTAL_ITEMS) {
            void *ptr = queue.pop_front();
            if (!ptr) {
                // queue 可能暂时为空
                std::this_thread::yield();
                continue;
            }

            int id = static_cast<int>(reinterpret_cast<intptr_t>(ptr));

            ASSERT_GT(id, 0);
            ASSERT_LE(id, TOTAL_ITEMS);

            int old = seen[id].fetch_add(1, std::memory_order_relaxed);
            ASSERT_EQ(old, 0) << "Duplicate pop detected for id=" << id;

            consumed.fetch_add(1, std::memory_order_release);
            ++pop_count;
        }
    };

    // 启动线程
    std::vector<std::thread> threads;

    for (int i = 0; i < PRODUCER_COUNT; ++i)
        threads.emplace_back(producer);

    for (int i = 0; i < CONSUMER_COUNT; ++i)
        threads.emplace_back(consumer);

    for (auto &t : threads)
        t.join();

    // 最终一致性检查
    EXPECT_EQ(consumed.load(), TOTAL_ITEMS);

    for (int i = 1; i <= TOTAL_ITEMS; ++i) {
        EXPECT_EQ(seen[i].load(), 1) << "Item " << i << " was not popped exactly once";
    }
} //*/
///*

constexpr int SEEN_TABLE_SIZE = 1 << 20;

static std::atomic<bool> running{true};

static void sigint_handler(int)
{
    running.store(false, std::memory_order_relaxed);
}

TEST(TSLinkedListTest, PushPopStressUntilInterrupted)
{
    std::signal(SIGINT, sigint_handler);

    TSLinkedList queue;
    queue.init();

    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> consumed{0};

    std::vector<std::atomic<uint64_t>> seen(SEEN_TABLE_SIZE);
    for (auto &v : seen)
        v.store(0, std::memory_order_relaxed);

    auto producer = [&]() {
        while (running.load(std::memory_order_relaxed)) {
            uint64_t id = produced.fetch_add(1, std::memory_order_relaxed) + 1;
            queue.push_back(reinterpret_cast<void *>(id));
        }
    };

    auto consumer = [&]() {
        while (running.load(std::memory_order_relaxed) ||
               consumed.load(std::memory_order_relaxed) <
                   produced.load(std::memory_order_relaxed)) {

            void *ptr = queue.pop_front();
            if (!ptr) {
                std::this_thread::yield();
                continue;
            }

            uint64_t id = reinterpret_cast<uint64_t>(ptr);
            ASSERT_GT(id, 0);

            uint64_t slot = id & (SEEN_TABLE_SIZE - 1);
            uint64_t old = seen[slot].load(std::memory_order_relaxed);
            // 如果同一个 id 被 pop 两次
            ASSERT_NE(old, id) << "Duplicate pop detected for id=" << id;

            seen[slot].store(id, std::memory_order_relaxed);

            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < PRODUCER_COUNT; ++i)
        threads.emplace_back(producer);
    for (int i = 0; i < CONSUMER_COUNT; ++i)
        threads.emplace_back(consumer);

    // 主线程定期打印状态
    while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint64_t p = produced.load();
        uint64_t c = consumed.load();
        std::cerr << "[stress] produced=" << p << " consumed=" << c << " in_flight=" << (p - c)
                  << " new_count=" << TSLinkedListNodeAllocator::inst.get_new_count() << "\n";
    }

    for (auto &t : threads)
        t.join();

    std::cerr << "[stress] stopped. produced=" << produced.load() << " consumed=" << consumed.load()
              << std::endl;

    EXPECT_EQ(consumed.load(), produced.load());
}
//*/