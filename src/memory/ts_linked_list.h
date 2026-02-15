#pragma once
#include <atomic>
#include "config.h"
#include "memory/tagged_ptr.h"

namespace rockcoro {

constexpr int TS_LINKED_LIST_NODE_CACHE_MAX_THREADS = 1024;
constexpr int TS_LINKED_LIST_NODE_CACHE_COUNT = 128;
constexpr int TS_LINKED_LIST_NODE_CACHE_BATCH_RELEASE_COUNT = 64;

struct TSLinkedListNode;
using TSLinkedListNodePtr = TaggedPtr<TSLinkedListNode>;

//thread safe linked list node
struct TSLinkedListNode {
    std::atomic<TSLinkedListNodePtr> next_;
    void *value_;
    //debug
    std::atomic<bool> released_ = false;

    TSLinkedListNode(void *value);
};

struct TSLinkedListNodeCache {
    TSLinkedListNodePtr head_ = {nullptr};
    int count_ = 0;

    void destroy();
    void push(TSLinkedListNodePtr node);
    TSLinkedListNodePtr pop();
};

class TSLinkedListNodeAllocator {
public:
    static TSLinkedListNodeAllocator inst;

private:
    TSLinkedListNodeCache tl_node_cache_[TS_LINKED_LIST_NODE_CACHE_MAX_THREADS];
    std::atomic<int> tl_node_cache_count_{0};

public:
    void init();
    void destroy();
    TSLinkedListNodePtr get();
    void release(TSLinkedListNodePtr node);
    //debug functions
    uint64_t get_new_count();

private:
    // free unused cached node in a batch
    void batch_release();
    // any threads that use NodeAllocator must call this function first (will be automatically called)
    void init_thread_local_cache();
};

//thread safe linked list. can be used only by scheduler
// threads that uses TSLinkedList must call EpochBaseReclamation::inst.init_thread_epoch (so EBR could help avoid ABA problem)
struct TSLinkedList {
private:
    std::atomic<TSLinkedListNodePtr> head_ = {{nullptr}};
    std::atomic<TSLinkedListNodePtr> tail_ = {{nullptr}};

public:
    TSLinkedList();
    ~TSLinkedList();
    void init();
    void destroy();
    // pop an element from head. returns nullptr if empty
    void *pop_front();
    void push_back(void *value);
};
} // namespace rockcoro