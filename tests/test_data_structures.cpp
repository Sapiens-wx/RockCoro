#include <atomic>
#include <gtest/gtest.h>
#include <stdio.h>
#include <thread>
#include <unordered_set>
#include <vector>
#include <mutex>
#include "basic_struct/data_structures.h"
#include "coroutine/coroutine.h"
#include "log.h"
#include "scheduler.h"

using namespace rockcoro;

constexpr const int NUM_WORKERS = 10, ITEMS_PER_WORKER = 100;
constexpr const int NUM_PUSH_PER_ITEM = 10;

struct Params {
    int id;
    LinkedList &list;

    std::atomic<int> &pop_count;

    std::mutex &co_push_count_mutex, &co_pop_count_mutex;
    std::unordered_map<Coroutine *, int> &co_push_count, &co_pop_count;

    int *completed_consumer;

    Params(int id,
           LinkedList &list,
           std::atomic<int> &pop_count,
           std::mutex &co_push_count_mutex,
           std::mutex &co_pop_count_mutex,
           std::unordered_map<Coroutine *, int> &co_push_count,
           std::unordered_map<Coroutine *, int> &co_pop_count,
           int *completed_consumer)
        : id(id)
        , list(list)
        , pop_count(pop_count)
        , co_push_count_mutex(co_push_count_mutex)
        , co_pop_count_mutex(co_pop_count_mutex)
        , co_push_count(co_push_count)
        , co_pop_count(co_pop_count)
        , completed_consumer(completed_consumer)
    {
    }
    Params(int id, const Params &tmp)
        : id(id)
        , list(tmp.list)
        , pop_count(tmp.pop_count)
        , co_push_count_mutex(tmp.co_push_count_mutex)
        , co_pop_count_mutex(tmp.co_pop_count_mutex)
        , co_push_count(tmp.co_push_count)
        , co_pop_count(tmp.co_pop_count)
        , completed_consumer(tmp.completed_consumer)
    {
    }
};

static void tl_linked_list_worker(void *args)
{
    Params *param = (Params *)args;
    //TODO: set list
    LinkedList *list = &param->list;

    // create list nodes to be added to the linked list
    Coroutine *coroutines[ITEMS_PER_WORKER];
    for (int i = 0; i < ITEMS_PER_WORKER; ++i) {
        coroutines[i] = new Coroutine(nullptr, nullptr);
    }
    int j = 0;

    while (param->pop_count.load() < NUM_WORKERS * ITEMS_PER_WORKER * NUM_PUSH_PER_ITEM) {
        Coroutine *front = list->pop_front();
        if (front) {
            param->pop_count++;
            param->co_pop_count_mutex.lock();
            param->co_pop_count[front]++;
            param->co_pop_count_mutex.unlock();
            param->co_push_count_mutex.lock();
            int co_push_count = param->co_push_count[front];
            param->co_push_count_mutex.unlock();
            if (co_push_count < NUM_PUSH_PER_ITEM) { // still can push this coroutine
                list->push_back(front);
                std::lock_guard<std::mutex> lock(param->co_push_count_mutex);
                param->co_push_count[front]++;
            }
        }
        // push jobs in [pending_adds]
        if (j < ITEMS_PER_WORKER) {
            list->push_back(coroutines[j]);
            std::lock_guard<std::mutex> lock(param->co_push_count_mutex);
            param->co_push_count[coroutines[j]]++;
            ++j;
        }
    }
	//release coroutines
	for(int i=0;i<ITEMS_PER_WORKER; ++i){
		delete coroutines[i];
	}
    param->completed_consumer[param->id] = 1;
    delete param;
};

TEST(TLLinkedListTest, PushPopTest)
{
    LinkedList list;
    std::atomic<int> pop_count{0};

    std::mutex co_push_count_mutex, co_pop_count_mutex;
    std::unordered_map<Coroutine *, int> co_push_count, co_pop_count;

    // completed array
    int completed_consumer[NUM_WORKERS];
    memset(completed_consumer, 0, sizeof(completed_consumer));

    // param
    Params tmp_param(0,
                     list,
                     pop_count,
                     co_push_count_mutex,
                     co_pop_count_mutex,
                     co_push_count,
                     co_pop_count,
                     completed_consumer);
    for (int i = 0; i < NUM_WORKERS; ++i) {
        std::thread t(tl_linked_list_worker, (void *)(new Params(i, tmp_param)));
        t.detach();
    }

    // wait for all threads to complete
    bool complete = false;
    while (complete == false) {
        complete = true;
        for (int i = 0; i < NUM_WORKERS; ++i) {
            if (completed_consumer[i] == 0) {
                complete = false;
            }
        }
        logf("waiting for coroutines to complete\n");
        sleep(1);
    }
    logf("all coroutines complete\n");

	int coroutine_count=0;
    for (auto &[key, value] : co_pop_count) {
        EXPECT_EQ(value, NUM_PUSH_PER_ITEM);
		++coroutine_count;
    }
	EXPECT_EQ(coroutine_count, NUM_WORKERS * ITEMS_PER_WORKER);
	coroutine_count=0;
    for (auto &[key, value] : co_push_count) {
        EXPECT_EQ(value, NUM_PUSH_PER_ITEM);
		++coroutine_count;
    }
	EXPECT_EQ(coroutine_count, NUM_WORKERS * ITEMS_PER_WORKER);
}
/*
TEST(DequeTest, PushPop)
{
    Deque<int> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0);
    for (int i = 0; i < 100; ++i) {
        q.push_back(i);
    }
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(q.size(), 100 - i);
        EXPECT_EQ(q.front(), i);
        EXPECT_FALSE(q.empty());
        q.pop_front();
    }
    for (int i = 100; i < 200; ++i) {
        q.push_front(i);
    }
    for (int i = 100; i < 200; ++i) {
        EXPECT_EQ(q.back(), i);
        EXPECT_FALSE(q.empty());
        q.pop_back();
    }
}
TEST(ChaseLevDequeTest, MultiThreadFinalTest)
{
    CLDeque<int> dq;

    const int NUM_CONSUMERS = 20;
    const int ITEMS_PER_PRODUCER = 20000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::mutex result_mutex;
    std::unordered_set<int> results; // 用于检测重复或丢失

    // print mutex
    std::mutex print_mutex;

    // theft thread: steal jobs using pop_front
    // if id==0, then consider it as main thread
    auto consumer = [&](int id) {
        int val;
        while (pop_count.load() < ITEMS_PER_PRODUCER) {
            const int *ptr = nullptr;
            ptr = id == 0 ? dq.pop_back() : dq.pop_front();

            if (ptr) {
                val = *ptr;
                std::lock_guard<std::mutex> lock(result_mutex);
                ASSERT_TRUE(results.insert(val).second) << "Duplicate value detected: " << val;
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

    for (auto &t : threads)
        t.join();

    // 验证最终结果
    EXPECT_EQ(push_count.load(), ITEMS_PER_PRODUCER);
    EXPECT_EQ(pop_count.load(), ITEMS_PER_PRODUCER);
    EXPECT_EQ(results.size(), ITEMS_PER_PRODUCER);
}
TEST(MSQueueTest, MultiThreadTest)
{
    MSQueue<int> q;

    const int NUM_CONSUMERS = 20, NUM_PRODUCERS = 20;
    const int ITEMS_PER_PRODUCER = 1000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::mutex result_mutex;
    std::unordered_set<int> results; // 用于检测重复或丢失

    // print mutex
    std::mutex print_mutex;

    // theft thread: steal jobs using pop_front
    // if id==0, then consider it as main thread
    auto consumer = [&](int id) {
        int val;
        while (pop_count.load() < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
            const int *ptr = nullptr;
            ptr = q.pop();

            if (ptr) {
                val = *ptr;
                std::lock_guard<std::mutex> lock(result_mutex);
                ASSERT_TRUE(results.insert(val).second) << "Duplicate value detected: " << val;
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

    for (auto &t : threads)
        t.join();

    // 验证最终结果
    EXPECT_EQ(push_count.load(), ITEMS_PER_PRODUCER * NUM_PRODUCERS);
    EXPECT_EQ(pop_count.load(), ITEMS_PER_PRODUCER * NUM_PRODUCERS);
    EXPECT_EQ(results.size(), ITEMS_PER_PRODUCER * NUM_PRODUCERS);
}
*/
