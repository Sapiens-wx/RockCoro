#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include "basic_struct/data_structures.h"

using namespace rockcoro;

TEST(DequeTest, PushPop){
    Deque<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0);
    for(int i=0;i<100;++i){
        q.push_back(i);
    }
    for(int i=0;i<100;++i){
        EXPECT_EQ(q.size(), 100-i);
        EXPECT_EQ(q.front(), i);
        EXPECT_FALSE(q.empty());
        q.pop_front();
    }
    for(int i=100;i<200;++i){
        q.push_front(i);
    }
    for(int i=100;i<200;++i){
        EXPECT_EQ(q.back(), i);
        EXPECT_FALSE(q.empty());
        q.pop_back();
    }
}
TEST(ChaseLevDequeTest, SingleThreadTest) {
    CLDeque<int, 2, 2> q;
    q.push_back(0);
    q.push_back(1);
    q.push_back(2);
    q.push_back(3);
    EXPECT_EQ(*q.pop_back(), 3);
    EXPECT_EQ(*q.pop_front(), 0);
    EXPECT_EQ(*q.pop_front(), 1);
    q.push_back(4);
    EXPECT_EQ(*q.pop_front(), 2);
    EXPECT_EQ(*q.pop_front(), 4);
    EXPECT_EQ(q.pop_back(), nullptr);
    EXPECT_EQ(q.pop_front(), nullptr);
}
TEST(ChaseLevDequeTest, MultiThreadTest){
    CLDeque<int> dq;

    const int NUM_PRODUCERS = 10;
    const int NUM_CONSUMERS = 10;
    const int ITEMS_PER_PRODUCER = 2000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::mutex result_mutex;
    std::unordered_set<int> results; // 用于检测重复或丢失

    // 生产者线程：每个生产者 push 一批唯一的数字
    auto producer = [&](int id) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            int val = id * ITEMS_PER_PRODUCER + i;
            dq.push_back(val);
            push_count++;
        }
    };

    // 消费者线程：从 front 或 back 随机 pop
    auto consumer = [&]() {
        int val;
        while (pop_count.load() < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
            const int* ptr=nullptr;
            if (rand() % 2 == 0)
                ptr=dq.pop_front();
            else
                ptr=dq.pop_back();

            if (ptr) {
                std::lock_guard<std::mutex> lock(result_mutex);
                ASSERT_TRUE(results.insert(val).second) 
                    << "Duplicate value detected: " << val;
                pop_count++;
            } else {
                std::this_thread::yield(); // 暂时无元素，yield
            }
        }
    };

    // 启动所有线程
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_PRODUCERS; ++i)
        threads.emplace_back(producer, i);
    for (int i = 0; i < NUM_CONSUMERS; ++i)
        threads.emplace_back(consumer);

    for (auto& t : threads)
        t.join();

    // 验证最终结果
    EXPECT_EQ(push_count.load(), NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    EXPECT_EQ(pop_count.load(), NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    EXPECT_EQ(results.size(), NUM_PRODUCERS * ITEMS_PER_PRODUCER);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
