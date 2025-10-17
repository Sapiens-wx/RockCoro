#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <stdio.h>
#include "basic_struct/data_structures.h"
#include "log.h"

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
TEST(ChaseLevDequeTest, MultiThreadFinalTest){
    CLDeque<int> dq;

    const int NUM_CONSUMERS = 20;
    const int ITEMS_PER_PRODUCER = 20000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::mutex result_mutex;
    std::unordered_set<int> results; // 用于检测重复或丢失

	//print mutex
	std::mutex print_mutex;

    // theft thread: steal jobs using pop_front
	// if id==0, then consider it as main thread
    auto consumer = [&](int id) {
        int val;
        while (pop_count.load() < ITEMS_PER_PRODUCER) {
            const int* ptr=nullptr;
			ptr=id==0?dq.pop_back():dq.pop_front();

            if (ptr) {
				val=*ptr;
                std::lock_guard<std::mutex> lock(result_mutex);
                ASSERT_TRUE(results.insert(val).second) 
                    << "Duplicate value detected: " << val;
                pop_count++;
            } else {
                std::this_thread::yield(); // 暂时无元素，yield
            }
        }
    };

    // 生产者线程：每个生产者 push 一批唯一的数字
    auto producer = [&](int id) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            int val = id * ITEMS_PER_PRODUCER + i;
            dq.push_back(val);
            push_count++;
			std::lock_guard<std::mutex> lock(print_mutex);
        }
		consumer(0);
    };

    // 启动所有线程
    std::vector<std::thread> threads;
	threads.emplace_back(producer, 0);
    for (int i = 1; i < NUM_CONSUMERS; ++i)
        threads.emplace_back(consumer, i);

    for (auto& t : threads)
        t.join();

    // 验证最终结果
    EXPECT_EQ(push_count.load(), ITEMS_PER_PRODUCER);
    EXPECT_EQ(pop_count.load(), ITEMS_PER_PRODUCER);
    EXPECT_EQ(results.size(), ITEMS_PER_PRODUCER);
}
TEST(MSQueueTest, MultiThreadTest){
    MSQueue<int> q;

    const int NUM_CONSUMERS = 20, NUM_PRODUCERS = 20;
    const int ITEMS_PER_PRODUCER = 1000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::mutex result_mutex;
    std::unordered_set<int> results; // 用于检测重复或丢失

	//print mutex
	std::mutex print_mutex;

    // theft thread: steal jobs using pop_front
	// if id==0, then consider it as main thread
    auto consumer = [&](int id) {
        int val;
        while (pop_count.load() < NUM_PRODUCERS*ITEMS_PER_PRODUCER) {
            const int* ptr=nullptr;
			ptr=q.pop();

            if (ptr) {
				val=*ptr;
                std::lock_guard<std::mutex> lock(result_mutex);
                ASSERT_TRUE(results.insert(val).second) 
                    << "Duplicate value detected: " << val;
                pop_count++;
            } else {
                std::this_thread::yield(); // 暂时无元素，yield
            }
        }
    };

    // 生产者线程：每个生产者 push 一批唯一的数字
    auto producer = [&](int id) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            int val = id * ITEMS_PER_PRODUCER + i;
            q.push(val);
            push_count++;
        }
    };

    // 启动所有线程
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_PRODUCERS; ++i)
        threads.emplace_back(producer, i);
    for (int i = 0; i < NUM_CONSUMERS; ++i)
        threads.emplace_back(consumer, i);

    for (auto& t : threads)
        t.join();

    // 验证最终结果
    EXPECT_EQ(push_count.load(), ITEMS_PER_PRODUCER*NUM_PRODUCERS);
    EXPECT_EQ(pop_count.load(), ITEMS_PER_PRODUCER*NUM_PRODUCERS);
    EXPECT_EQ(results.size(), ITEMS_PER_PRODUCER*NUM_PRODUCERS);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
