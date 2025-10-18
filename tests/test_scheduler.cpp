#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <stdio.h>
#include "basic_struct/data_structures.h"
#include "log.h"
#include "scheduler.h"

using namespace rockcoro;

constexpr const int NUM_CONSUMERS = 100, NUM_PRODUCERS = 100;
constexpr const int ITEMS_PER_PRODUCER = 1000;

struct Params
{
	int id;
    MSQueue<int>& q;

    std::atomic<int>& push_count;
    std::atomic<int>& pop_count;

    std::mutex& result_mutex;
    std::unordered_set<int>& results; // 用于检测重复或丢失

	//print mutex
	std::mutex& print_mutex;

	int* completed_producer, *completed_consumer;

	Params(int id, MSQueue<int>& q, std::atomic<int>& push_count, std::atomic<int>& pop_count, std::mutex& result_mutex, std::unordered_set<int>& results, std::mutex& print_mutex, int* completed_producer, int* completed_consumer):
		id(id), q(q), push_count(push_count), pop_count(pop_count), 
		result_mutex(result_mutex), results(results),
		print_mutex(print_mutex), completed_producer(completed_producer), completed_consumer(completed_consumer){
	}
	Params(int id, const Params& tmp):
		id(id), q(tmp.q), push_count(tmp.push_count), pop_count(tmp.pop_count), 
		result_mutex(tmp.result_mutex), results(tmp.results),
		print_mutex(print_mutex), completed_producer(tmp.completed_producer), completed_consumer(tmp.completed_consumer){
	}
};

void consumer(void* args)
{
	Params* param=(Params*)args;
	int val;
	while (param->pop_count.load() < NUM_PRODUCERS*ITEMS_PER_PRODUCER) {
		const int* ptr=nullptr;
		ptr=param->q.pop();

		if (ptr) {
			val=*ptr;
			std::lock_guard<std::mutex> lock(param->result_mutex);
			ASSERT_TRUE(param->results.insert(val).second) 
				<< "Duplicate value detected: " << val;
			param->pop_count++;
		} else {
		}
		Scheduler::inst.coroutine_yield();
	}
	param->completed_consumer[param->id]=1;
	delete param;
};

// 生产者线程：每个生产者 push 一批唯一的数字
static void producer(void* args)
{
	Params* param=(Params*)args;
	for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
		int val = param->id * ITEMS_PER_PRODUCER + i;
		param->q.push(val);
		param->push_count++;
		Scheduler::inst.coroutine_yield();
	}
	param->completed_producer[param->id]=1;
	delete param;
};

TEST(SchedulerTest, CoroutineTest)
{
    MSQueue<int> q;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::mutex result_mutex;
    std::unordered_set<int> results; // 用于检测重复或丢失

	//print mutex
	std::mutex print_mutex;

	//completed array
	int completed_producer[NUM_PRODUCERS];
	int completed_consumer[NUM_CONSUMERS];
	memset(completed_producer, 0, sizeof(completed_producer));
	memset(completed_consumer, 0, sizeof(completed_consumer));

	//param
	Params tmp_param(0, q, push_count, pop_count, result_mutex, results, print_mutex,
		completed_producer, completed_consumer);

    // 启动所有线程
    for (int i = 0; i < NUM_PRODUCERS; ++i)
		Scheduler::inst.coroutine_create(&producer, new Params(i, tmp_param));
    for (int i = 0; i < NUM_CONSUMERS; ++i){
		Params* p=new Params(i, tmp_param);
		Scheduler::inst.coroutine_create(&consumer, p);
	}
	
	// wait for all threads to complete
	bool complete=false;
	while(complete==false){
		complete=true;
		for(int i=0;i<NUM_PRODUCERS;++i)
			if(completed_producer[i]==0)
				complete=false;
		for(int i=0;i<NUM_CONSUMERS;++i)
			if(completed_consumer[i]==0)
				complete=false;
		logf("waiting for coroutines to complete\n");
		sleep(1);
	}
	logf("all coroutines complete\n");

    // 验证最终结果
    EXPECT_EQ(push_count.load(), ITEMS_PER_PRODUCER*NUM_PRODUCERS);
    EXPECT_EQ(pop_count.load(), ITEMS_PER_PRODUCER*NUM_PRODUCERS);
    EXPECT_EQ(results.size(), ITEMS_PER_PRODUCER*NUM_PRODUCERS);
}
