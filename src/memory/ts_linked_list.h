#pragma once
#include <atomic>
#include "config.h"

namespace rockcoro {

//thread safe linked list node
struct TSLinkedListNode {
    std::atomic<TSLinkedListNode *> next;
    void *value;
    std::atomic<bool> released = false;
    std::atomic<int> used_by_thread_epoch = -1;

    TSLinkedListNode(void *value);
};

struct TSLinkedListNodeCache {
    TSLinkedListNode *head = nullptr;
    int count = 0;

    void destroy();
    void push(TSLinkedListNode *node);
    TSLinkedListNode *pop();
};

class TSLinkedListNodeAllocator {
public:
    static TSLinkedListNodeAllocator inst;
    TSLinkedListNodeCache tl_node_cache[TS_LINKED_LIST_NODE_CACHE_MAX_THREADS];
    std::atomic<int> tl_node_cache_count{0};

    void init();
    void destroy();
    TSLinkedListNode *get();
    void release(TSLinkedListNode *node);
    // any threads that use NodeAllocator must call this function first
    void init_thread_local_cache();
    //debug functions
    uint64_t get_new_count();

private:
    void batch_release();
};

//thread safe linked list. can be used only by scheduler
// threads that uses TSLinkedList must call EpochBaseReclamation::inst.init_thread_epoch (so EBR could help avoid ABA problem)
struct TSLinkedList {
    std::atomic<TSLinkedListNode *> head = nullptr;
    std::atomic<TSLinkedListNode *> tail = nullptr;

    TSLinkedList();
    ~TSLinkedList();
    void init();
    void destroy();
    // pop an element from head. returns nullptr if empty
    void *pop_front();
    void push_back(void *value);
};
} // namespace rockcoro