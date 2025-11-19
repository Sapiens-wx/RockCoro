#include <atomic>
#include <gtest/gtest.h>
#include <stdio.h>
#include <thread>
#include <unordered_set>
#include <vector>
#include "basic_struct/data_structures.h"
#include "log.h"
#include "scheduler.h"
#include "src/coroutine/coroutine.h"

using namespace rockcoro;

constexpr const int NUM_WORKERS = 100, ITEMS_PER_WORKER = 100;
constexpr const int NUM_PUSH_PER_ITEM = 10;

struct Params {
    int id;
    TLLinkedList &list;

    std::atomic<int> &pop_count;

    std::mutex &co_push_count_mutex, &co_pop_count_mutex;
    std::unordered_map<Coroutine *, int> &co_push_count, &co_pop_count;

    int *completed_consumer;

    Params(int id,
           TLLinkedList &list,
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

void consumer(void *args)
{
    Params *param = (Params *)args;
    //TODO: set list
    TLLinkedList *list = param->list;

    // create list nodes to be added to the linked list
    Coroutine *coroutines[ITEMS_PER_WORKER];
    for (int i = 0; i < ITEMS_PER_WORKER; ++i) {
        coroutines[i] = new Coroutine(nullptr, nullptr);
    }
    int j = 0;

    while (param->pop_count.load() < NUM_WORKERS * ITEMS_PER_WORKER * NUM_PUSH_PER_ITEM) {
        Coroutine *front = list->pop_front();
        if (front) {
            pop_count++;
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
            list->push_back(&coroutines[j]);
            std::lock_guard<std::mutex> lock(param->co_push_count_mutex);
            param->co_push_count[&coroutines[j]]++;
            ++j;
        }
    }
    param->completed_consumer[param->id] = 1;
    delete param;
};

TEST(SchedulerTest, CoroutineTest)
{
    TLLinkedList list;
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
        std::thread(consumer, new Param(i, tmp_param));
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
    for (auto &[key, value] : m) {
        EXPECT_EQ(value, NUM_PUSH_PER_ITEM);
    }
}
