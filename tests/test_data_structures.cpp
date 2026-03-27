#include <algorithm>
#include <assert.h>
#include <atomic>
#include <csignal>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "basic_struct/red_black_tree.h"
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

/*
TEST(TaggedPointerTest, CorrectTag)
{
    struct Dummy {
        int value;
    };
    Dummy d{42};

    // 1️⃣ 默认构造
    TaggedPtr<Dummy> tp_default;
    EXPECT_EQ(tp_default.get_ptr(), nullptr);
    EXPECT_EQ(tp_default.get_tag(), 0);

    // 2️⃣ ptr 构造
    TaggedPtr<Dummy> tp1(&d);
    EXPECT_EQ(tp1.get_ptr(), &d);
    EXPECT_EQ(tp1.get_tag(), 0);

    // 3️⃣ ptr + tag 构造
    TaggedPtr<Dummy> tp2(&d, 5);
    EXPECT_EQ(tp2.get_ptr(), &d);
    EXPECT_EQ(tp2.get_tag(), 5);

    // 4️⃣ operator->
    EXPECT_EQ(tp2->value, 42);

    // 5️⃣ operator*
    EXPECT_EQ((*tp2).value, 42);

    // 6️⃣ increment_tag
    tp2.increment_tag();
    EXPECT_EQ(tp2.get_tag(), 6);
    EXPECT_EQ(tp2.get_ptr(), &d);

    // 7️⃣ 多次递增
    for (int i = 0; i < 10; ++i)
        tp2.increment_tag();

    EXPECT_EQ(tp2.get_tag(), 16);
    EXPECT_EQ(tp2.get_ptr(), &d);
}
*/

/*
TEST(TSLinkedListTest, PushPopStressUntilInterrupted)
{
    std::signal(SIGINT, sigint_handler);

    TSLinkedList<Coroutine> queue;
    queue.init();

    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> consumed{0};

    std::vector<std::atomic<uint64_t>> seen(SEEN_TABLE_SIZE);
    for (auto &v : seen)
        v.store(0, std::memory_order_relaxed);

    auto producer = [&]() {
        while (running.load(std::memory_order_relaxed)) {
            uint64_t id = produced.fetch_add(1, std::memory_order_relaxed) + 1;
            queue.push_back(reinterpret_cast<Coroutine *>(id));
        }
    };

    auto consumer = [&]() {
        while (running.load(std::memory_order_relaxed) ||
               consumed.load(std::memory_order_relaxed) <
                   produced.load(std::memory_order_relaxed)) {

            Coroutine *ptr = queue.pop_front();
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

// ===== Comparer =====
struct IntPtrComparer {
    bool operator()(int *a, int *b) const
    {
        return *a < *b;
    }
};

// ===== Test Fixture =====
class RBTreeTest : public ::testing::Test {
protected:
    RBTree<int, IntPtrComparer> tree;
    std::vector<int *> allocated; // 统一管理内存

    int *make(int v)
    {
        int *p = new int(v);
        allocated.push_back(p);
        return p;
    }

    void TearDown() override
    {
        for (auto p : allocated) {
            delete p;
        }
        allocated.clear();
    }
};

// ===== 基础插入 + 查找 =====
TEST_F(RBTreeTest, InsertAndFind)
{
    int *a = make(10);
    int *b = make(20);
    int *c = make(5);

    EXPECT_TRUE(tree.insert(a));
    EXPECT_TRUE(tree.insert(b));
    EXPECT_TRUE(tree.insert(c));

    EXPECT_EQ(*tree.find(a), 10);
    EXPECT_EQ(*tree.find(b), 20);
    EXPECT_EQ(*tree.find(c), 5);
}

// ===== 重复插入 =====
TEST_F(RBTreeTest, DuplicateInsert)
{
    int *a1 = make(10);
    int *a2 = make(10);

    EXPECT_TRUE(tree.insert(a1));
    EXPECT_FALSE(tree.insert(a2));
}

// ===== 删除 =====
TEST_F(RBTreeTest, EraseBasic)
{
    int *a = make(10);
    int *b = make(20);
    int *c = make(5);

    tree.insert(a);
    tree.insert(b);
    tree.insert(c);

    EXPECT_TRUE(tree.erase(b));
    EXPECT_EQ(tree.find(b), nullptr);

    EXPECT_TRUE(tree.erase(a));
    EXPECT_EQ(tree.find(a), nullptr);

    EXPECT_TRUE(tree.erase(c));
    EXPECT_EQ(tree.find(c), nullptr);
}

// ===== 删除不存在 =====
TEST_F(RBTreeTest, EraseNonExist)
{
    int *a = make(10);
    int *b = make(20);

    tree.insert(a);

    EXPECT_FALSE(tree.erase(b));
}

// ===== 最小 / 最大 =====
TEST_F(RBTreeTest, MinMax)
{
    std::vector<int *> vals = {make(10), make(20), make(5), make(15)};

    for (auto v : vals) {
        tree.insert(v);
    }

    EXPECT_EQ(*tree.minimum(), 5);
    EXPECT_EQ(*tree.maximum(), 20);
}

// ===== 顺序性（BST性质）=====
TEST_F(RBTreeTest, InOrderProperty)
{
    std::vector<int *> vals = {make(10), make(20), make(5), make(15), make(1)};

    for (auto v : vals) {
        tree.insert(v);
    }

    std::vector<int> result;

    while (true) {
        int *min = tree.minimum();
        if (!min)
            break;

        result.push_back(*min);
        tree.erase(min);
    }

    EXPECT_TRUE(std::is_sorted(result.begin(), result.end()));
}

// ===== 空树 =====
TEST_F(RBTreeTest, EmptyTree)
{
    int *tmp = make(10);

    EXPECT_EQ(tree.find(tmp), nullptr);
    EXPECT_EQ(tree.minimum(), nullptr);
    EXPECT_EQ(tree.maximum(), nullptr);
    EXPECT_FALSE(tree.erase(tmp));
}

// ===== 随机测试（去重版本）=====
TEST_F(RBTreeTest, RandomInsertErase_NoDuplicates)
{
    std::vector<int *> vals;
    std::unordered_set<int> used; // 用于去重

    std::mt19937 rng(123);
    std::uniform_int_distribution<int> dist(1, 1000);

    // ===== 插入（自动去重）=====
    for (int i = 0; i < 300; i++) {
        int v = dist(rng);

        if (used.count(v))
            continue; // 🔥 丢弃重复值

        used.insert(v);

        int *p = make(v);
        vals.push_back(p);

        EXPECT_TRUE(tree.insert(p));
    }

    // ===== 查找验证 =====
    for (auto p : vals) {
        int *res = tree.find(p);
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(*res, *p);
    }

    // ===== 删除一半 =====
    for (int i = 0; i < (int)vals.size(); i += 2) {
        EXPECT_TRUE(tree.erase(vals[i]));
    }

    // ===== 再验证 =====
    for (int i = 0; i < (int)vals.size(); i++) {
        int *res = tree.find(vals[i]);

        if (i % 2 == 0) {
            EXPECT_EQ(res, nullptr);
        } else {
            ASSERT_NE(res, nullptr);
            EXPECT_EQ(*res, *vals[i]);
        }
    }
}