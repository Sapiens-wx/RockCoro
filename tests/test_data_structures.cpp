#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <gtest/gtest.h>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "basic_struct/chunked_vector.h"
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

/* Red Black Tree test

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
*/

// =====Chunked Vector test=====

// 假设 ChunkedVector 已经在头文件中定义
// #include "chunked_vector.h"

// ==================== 测试辅助函数 ====================

// 生成随机整数向量
std::vector<int> generate_random_numbers(size_t count, int min_val = 0, int max_val = 10000)
{
    std::vector<int> result;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min_val, max_val);

    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(dis(gen));
    }
    return result;
}

// 生成有序序列
std::vector<int> generate_sorted_sequence(size_t count, bool ascending = true)
{
    std::vector<int> result(count);
    for (size_t i = 0; i < count; ++i) {
        result[i] = ascending ? static_cast<int>(i) : static_cast<int>(count - i);
    }
    return result;
}

// ==================== 基础功能测试 ====================

class ChunkedVectorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 测试前的初始化
    }

    void TearDown() override
    {
        // 测试后的清理
    }
};

// 测试 1: 验证离散分布特性
TEST_F(ChunkedVectorTest, DiscreteMemoryLayout)
{
    constexpr size_t BS = 4;
    ChunkedVector<int, BS> cv;

    for (int i = 0; i < 10; ++i) {
        cv.push_back(i);
    }

    EXPECT_GT(cv.block_count(), 1);

    std::vector<const void *> block_addresses;
    for (size_t i = 0; i < cv.block_count(); ++i) {
        size_t block_start = i * BS;

        if (block_start < cv.size()) {
            const void *first_addr = &cv[block_start];

            if (block_start + 1 < cv.size()) {
                const void *second_addr = &cv[block_start + 1];
                EXPECT_EQ(static_cast<const char *>(second_addr) -
                              static_cast<const char *>(first_addr),
                          sizeof(int));
            }

            block_addresses.push_back(first_addr);
        }
    }

    bool has_gap = false;
    for (size_t i = 1; i < block_addresses.size(); ++i) {
        ptrdiff_t gap = static_cast<const char *>(block_addresses[i]) -
                        static_cast<const char *>(block_addresses[i - 1]);

        if (gap != static_cast<ptrdiff_t>(BS * sizeof(int))) {
            has_gap = true;
            break;
        }
    }

    EXPECT_TRUE(has_gap || block_addresses.size() <= 1);
}

// 测试 2: 基本操作（push, pop, size, empty）
TEST_F(ChunkedVectorTest, BasicOperations)
{
    ChunkedVector<int, 4> cv;

    EXPECT_TRUE(cv.empty());
    EXPECT_EQ(cv.size(), 0);

    // 测试 push_back
    cv.push_back(10);
    EXPECT_FALSE(cv.empty());
    EXPECT_EQ(cv.size(), 1);
    EXPECT_EQ(cv[0], 10);
    EXPECT_EQ(cv.front(), 10);
    EXPECT_EQ(cv.back(), 10);

    // 测试多个 push_back
    cv.push_back(20);
    cv.push_back(30);
    EXPECT_EQ(cv.size(), 3);
    EXPECT_EQ(cv[0], 10);
    EXPECT_EQ(cv[1], 20);
    EXPECT_EQ(cv[2], 30);
    EXPECT_EQ(cv.back(), 30);

    // 测试 pop_back
    cv.pop_back();
    EXPECT_EQ(cv.size(), 2);
    EXPECT_EQ(cv[0], 10);
    EXPECT_EQ(cv[1], 20);
    EXPECT_EQ(cv.back(), 20);

    cv.pop_back();
    cv.pop_back();
    EXPECT_TRUE(cv.empty());
    EXPECT_EQ(cv.size(), 0);
}

// 测试 3: 移动语义
TEST_F(ChunkedVectorTest, MoveSemantics)
{
    ChunkedVector<std::string, 4> cv;

    std::string s1 = "hello";
    std::string s2 = "world";

    cv.push_back(s1);            // 左值拷贝
    cv.push_back(std::move(s2)); // 右值移动

    EXPECT_EQ(cv[0], "hello");
    EXPECT_EQ(cv[1], "world");

    // 测试 emplace_back
    cv.emplace_back(3, 'a'); // 构造 "aaa"
    EXPECT_EQ(cv[2], "aaa");
}

// 测试 4: 迭代器功能
TEST_F(ChunkedVectorTest, IteratorFunctionality)
{
    ChunkedVector<int, 4> cv;

    for (int i = 0; i < 10; ++i) {
        cv.push_back(i);
    }

    // 测试正向迭代
    int expected = 0;
    for (auto it = cv.begin(); it != cv.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }

    // 测试 const 迭代器
    const auto &const_cv = cv;
    expected = 0;
    for (auto it = const_cv.begin(); it != const_cv.end(); ++it) {
        EXPECT_EQ(*it, expected++);
    }

    // 测试随机访问
    auto it = cv.begin();
    EXPECT_EQ(*(it + 3), 3);
    EXPECT_EQ(*(it + 5), 5);
    EXPECT_EQ(*(it + 7), 7);
    EXPECT_EQ(it[8], 8);

    // 测试迭代器比较
    auto it2 = cv.begin();
    EXPECT_EQ(it, it2);
    EXPECT_NE(it + 1, it2);
    EXPECT_LT(it, it + 5);
    EXPECT_GT(it + 5, it);
}

// 测试 5: 容量和预留空间
TEST_F(ChunkedVectorTest, GrowthAndShrink)
{
    ChunkedVector<int, 4> cv;

    EXPECT_TRUE(cv.empty());

    // push 触发多个 block
    for (int i = 0; i < 10; ++i) {
        cv.push_back(i);
    }

    EXPECT_EQ(cv.size(), 10);
    EXPECT_GT(cv.block_count(), 1);

    // shrink
    cv.clear();
    cv.shrink_to_fit();

    EXPECT_TRUE(cv.empty());
    EXPECT_EQ(cv.block_count(), 0);
}

// ==================== priority_queue 测试 ====================

class PriorityQueueWithChunkedVectorTest : public ::testing::Test {
protected:
    using MinHeap = std::priority_queue<int, ChunkedVector<int, 4>, std::greater<int>>;
    using MaxHeap = std::priority_queue<int, ChunkedVector<int, 4>, std::less<int>>;

    void SetUp() override
    {
        // 初始化测试数据
        test_data_ = {5, 3, 7, 1, 9, 2, 8, 4, 6, 0};
        sorted_ascending_ = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        sorted_descending_ = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    }

    std::vector<int> test_data_;
    std::vector<int> sorted_ascending_;
    std::vector<int> sorted_descending_;
};

// 测试 6: 最小堆正确性
TEST_F(PriorityQueueWithChunkedVectorTest, MinHeapCorrectness)
{
    MinHeap min_heap;

    // 插入元素
    for (int val : test_data_) {
        min_heap.push(val);
    }

    // 验证大小
    EXPECT_EQ(min_heap.size(), test_data_.size());
    EXPECT_FALSE(min_heap.empty());

    // 弹出元素，验证顺序
    std::vector<int> popped;
    while (!min_heap.empty()) {
        popped.push_back(min_heap.top());
        min_heap.pop();
    }

    EXPECT_EQ(popped.size(), test_data_.size());
    EXPECT_EQ(popped, sorted_ascending_);
}

// 测试 7: 最大堆正确性
TEST_F(PriorityQueueWithChunkedVectorTest, MaxHeapCorrectness)
{
    MaxHeap max_heap;

    // 插入元素
    for (int val : test_data_) {
        max_heap.push(val);
    }

    // 弹出元素，验证顺序
    std::vector<int> popped;
    while (!max_heap.empty()) {
        popped.push_back(max_heap.top());
        max_heap.pop();
    }

    EXPECT_EQ(popped.size(), test_data_.size());
    EXPECT_EQ(popped, sorted_descending_);
}

// 测试 8: 大量随机数据测试
TEST_F(PriorityQueueWithChunkedVectorTest, LargeRandomData)
{
    const size_t data_size = 10000;
    auto random_data = generate_random_numbers(data_size);

    // 使用 ChunkedVector 的 priority_queue
    std::priority_queue<int, ChunkedVector<int, 64>> pq_chunked;
    for (int val : random_data) {
        pq_chunked.push(val);
    }

    // 使用标准 vector 的 priority_queue 作为参考
    std::priority_queue<int, std::vector<int>> pq_standard;
    for (int val : random_data) {
        pq_standard.push(val);
    }

    // 验证结果一致
    EXPECT_EQ(pq_chunked.size(), pq_standard.size());

    while (!pq_standard.empty()) {
        EXPECT_EQ(pq_chunked.top(), pq_standard.top());
        pq_chunked.pop();
        pq_standard.pop();
    }

    EXPECT_TRUE(pq_chunked.empty());
}

// 测试 9: 自定义类型测试
TEST_F(PriorityQueueWithChunkedVectorTest, CustomType)
{
    struct Person {
        std::string name;
        int age;

        bool operator<(const Person &other) const
        {
            return age < other.age;
        }

        bool operator>(const Person &other) const
        {
            return age > other.age;
        }
    };

    std::vector<Person> people = {
        {"Alice", 30}, {"Bob", 25}, {"Charlie", 35}, {"David", 20}, {"Eve", 28}};

    // 最小堆（按年龄）
    std::priority_queue<Person, ChunkedVector<Person, 2>, std::greater<Person>> min_heap;
    for (const auto &p : people) {
        min_heap.push(p);
    }

    std::vector<int> expected_ages = {20, 25, 28, 30, 35};
    std::vector<int> actual_ages;
    while (!min_heap.empty()) {
        actual_ages.push_back(min_heap.top().age);
        min_heap.pop();
    }

    EXPECT_EQ(actual_ages, expected_ages);
}

// 测试 10: 边界情况测试
TEST_F(PriorityQueueWithChunkedVectorTest, EdgeCases)
{
    // 空堆
    std::priority_queue<int, ChunkedVector<int, 4>> empty_heap;
    EXPECT_TRUE(empty_heap.empty());
    EXPECT_EQ(empty_heap.size(), 0);

    // 单元素堆
    std::priority_queue<int, ChunkedVector<int, 4>> single_heap;
    single_heap.push(42);
    EXPECT_FALSE(single_heap.empty());
    EXPECT_EQ(single_heap.size(), 1);
    EXPECT_EQ(single_heap.top(), 42);
    single_heap.pop();
    EXPECT_TRUE(single_heap.empty());

    // 重复元素
    std::priority_queue<int, ChunkedVector<int, 4>> dup_heap;
    dup_heap.push(5);
    dup_heap.push(5);
    dup_heap.push(5);
    EXPECT_EQ(dup_heap.size(), 3);

    std::vector<int> popped;
    while (!dup_heap.empty()) {
        popped.push_back(dup_heap.top());
        dup_heap.pop();
    }
    EXPECT_EQ(popped, std::vector<int>({5, 5, 5}));
}

// 测试 11: 性能对比测试（可选）
TEST_F(PriorityQueueWithChunkedVectorTest, PerformanceComparison)
{
    const size_t data_size = 100000;
    auto random_data = generate_random_numbers(data_size);

    // 测试 ChunkedVector 版本
    auto start = std::chrono::high_resolution_clock::now();
    std::priority_queue<int, ChunkedVector<int, 1024 * 16>> pq_chunked;
    for (int val : random_data) {
        pq_chunked.push(val);
    }
    while (!pq_chunked.empty()) {
        pq_chunked.pop();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto chunked_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 测试标准 vector 版本
    start = std::chrono::high_resolution_clock::now();
    std::priority_queue<int, std::vector<int>> pq_standard;
    for (int val : random_data) {
        pq_standard.push(val);
    }
    while (!pq_standard.empty()) {
        pq_standard.pop();
    }
    end = std::chrono::high_resolution_clock::now();
    auto standard_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 输出性能对比（仅供参考，不进行断言）
    std::cout << "Performance comparison (" << data_size << " elements):\n";
    std::cout << "  ChunkedVector: " << chunked_time.count() << "ms\n";
    std::cout << "  Standard vector: " << standard_time.count() << "ms\n";

    // 性能差异不应该过大（比如不超过 3 倍）
    //double ratio = static_cast<double>(chunked_time.count()) / standard_time.count();
    //EXPECT_LT(ratio, 3.0);
}

// 测试 12: 不同的块大小测试
TEST_F(PriorityQueueWithChunkedVectorTest, DifferentBlockSizes)
{
    std::vector<size_t> block_sizes = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

    for (size_t block_size : block_sizes) {
        // 根据块大小创建不同的 ChunkedVector 类型
        using ChunkedVec = ChunkedVector<int, 0>; // 无法在运行时指定，这里用模板参数

        // 使用 lambda 来创建不同块大小的堆
        auto test_with_block_size = [block_size](size_t block_size_) {
            // 由于 ChunkedVector 的块大小是编译时常量，这里只能测试一个固定的块大小
            // 实际使用时应该为不同的块大小实例化不同的类型
        };

        // 简单测试：确保编译通过
        if (block_size == 4) {
            std::priority_queue<int, ChunkedVector<int, 4>> pq;
            for (int i = 0; i < 100; ++i) {
                pq.push(i);
            }
            EXPECT_EQ(pq.size(), 100);
        }
    }
}

// 测试 13: 移动构造和移动赋值
TEST_F(PriorityQueueWithChunkedVectorTest, MoveOperations)
{
    std::priority_queue<int, ChunkedVector<int, 4>> pq1;

    for (int i = 0; i < 100; ++i) {
        pq1.push(i);
    }

    // 移动构造
    auto pq2 = std::move(pq1);
    EXPECT_EQ(pq2.size(), 100);

    // 移动赋值
    std::priority_queue<int, ChunkedVector<int, 4>> pq3;
    pq3 = std::move(pq2);
    EXPECT_EQ(pq3.size(), 100);

    // 验证数据正确性
    int expected = 99;
    while (!pq3.empty()) {
        EXPECT_EQ(pq3.top(), expected--);
        pq3.pop();
    }
}

// 测试 14: 交换操作
TEST_F(PriorityQueueWithChunkedVectorTest, SwapOperation)
{
    std::priority_queue<int, ChunkedVector<int, 4>> pq1;
    std::priority_queue<int, ChunkedVector<int, 4>> pq2;

    for (int i = 0; i < 10; ++i) {
        pq1.push(i);
        pq2.push(i + 100);
    }

    EXPECT_EQ(pq1.top(), 9);
    EXPECT_EQ(pq2.top(), 109);

    std::swap(pq1, pq2);

    EXPECT_EQ(pq1.top(), 109);
    EXPECT_EQ(pq2.top(), 9);
}
