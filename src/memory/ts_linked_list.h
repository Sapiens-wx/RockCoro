#pragma once
#include <atomic>
#include "config.h"
#include "memory/epoch_based_reclamation.h"
#include "memory/tagged_ptr.h"
#include "ts_linked_list_node.h"

namespace rockcoro {

constexpr int TS_LINKED_LIST_NODE_CACHE_MAX_THREADS = 1024;
constexpr int TS_LINKED_LIST_NODE_CACHE_COUNT = 128;
constexpr int TS_LINKED_LIST_NODE_CACHE_BATCH_RELEASE_COUNT = 64;

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
template <typename T> struct TSLinkedList {
private:
    std::atomic<TSLinkedListNodePtr> head_ = {{nullptr}};
    std::atomic<TSLinkedListNodePtr> tail_ = {{nullptr}};

public:
    TSLinkedList();
    ~TSLinkedList();
    void init();
    void destroy();
    // pop an element from head. returns nullptr if empty
    T *pop_front();
    void push_back(T *value);
};

template <typename T> TSLinkedList<T>::TSLinkedList()
{
}
template <typename T> TSLinkedList<T>::~TSLinkedList()
{
    destroy();
}
template <typename T> void TSLinkedList<T>::init()
{
    if (head_.load(std::memory_order_relaxed).get_ptr() != nullptr)
        return; //already initialized
    TSLinkedListNodePtr dummy = TSLinkedListNodeAllocator::inst.get();
    head_.store(dummy, std::memory_order_relaxed);
    tail_.store(dummy, std::memory_order_relaxed);
}
template <typename T> void TSLinkedList<T>::destroy()
{
    if (head_.load().get_ptr() != nullptr) {
        EpochBasedReclamation::inst.enter_epoch();
        //use EBR to release the dummy node
        for (TSLinkedListNodePtr cur = head_.load(std::memory_order_acquire);
             cur.get_ptr() != nullptr;
             cur = cur->next_.load()) {
            EpochBasedReclamation::inst.retire(cur);
        }
        head_.store(nullptr, std::memory_order_relaxed);
        tail_.store(nullptr, std::memory_order_relaxed);
        EpochBasedReclamation::inst.exit_epoch();
    }
}
template <typename T> T *TSLinkedList<T>::pop_front()
{
    init();
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNodePtr first = head_.load(std::memory_order_acquire);
        TSLinkedListNodePtr last = tail_.load(std::memory_order_acquire);
        assert(!first->released_.load(std::memory_order_relaxed));
        assert(!last->released_.load(std::memory_order_relaxed));
        TSLinkedListNodePtr next = first->next_.load(std::memory_order_acquire);
        //assert(next == nullptr || !next->released.load());
        if (first.get_ptr() == last.get_ptr()) {
            if (next.get_ptr() == nullptr) { // queue is empty
                EpochBasedReclamation::inst.exit_epoch();
                return nullptr;
            }
            tail_.compare_exchange_weak(
                last,
                next,
                std::memory_order_acq_rel,
                std::memory_order_relaxed); // tail is falling behind. update tail
        } else if (next.get_ptr() != nullptr) {
            T *value = reinterpret_cast<T *>(next->value_);
            if (head_.compare_exchange_strong(
                    first, next, std::memory_order_release, std::memory_order_relaxed)) {
                assert(first.get_ptr() != next.get_ptr()); //make sure not self-loop
                EpochBasedReclamation::inst.retire(first);
                EpochBasedReclamation::inst.exit_epoch();
                return value;
            }
        }
        EpochBasedReclamation::inst.exit_epoch();
    }
}
template <typename T> void TSLinkedList<T>::push_back(T *value)
{
    init();
    TSLinkedListNodePtr node = TSLinkedListNodeAllocator::inst.get();
    node.increment_tag();
    node->value_ = value;
    node->next_.store(nullptr);
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNodePtr last = tail_.load(std::memory_order_relaxed);
        TSLinkedListNodePtr next = last->next_.load(std::memory_order_acquire);
        if (next.get_ptr() == nullptr) {
            if (last->next_.compare_exchange_weak(
                    next, node, std::memory_order_release, std::memory_order_relaxed)) {
                assert(last.get_ptr() != node.get_ptr()); //make sure not self-loop
                tail_.compare_exchange_weak(
                    last, node, std::memory_order_release, std::memory_order_relaxed);
                EpochBasedReclamation::inst.exit_epoch();
                break;
            }
        } else
            tail_.compare_exchange_weak(
                last, next, std::memory_order_release, std::memory_order_relaxed);
        EpochBasedReclamation::inst.exit_epoch();
    }
}
} // namespace rockcoro