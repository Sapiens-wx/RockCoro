#include "memory/ts_linked_list.h"
#include <cassert>
#include <cstdlib>
#include <thread>
#include "memory/epoch_based_reclamation.h"

namespace rockcoro {

static thread_local int ts_node_cache_index = -1;

TSLinkedListNode::TSLinkedListNode(void *value)
    : value_(value)
{
    next_.store(nullptr, std::memory_order_relaxed);
}

TSLinkedList::TSLinkedList()
{
}
TSLinkedList::~TSLinkedList()
{
    destroy();
}
void TSLinkedList::init()
{
    if (head_.load() != nullptr)
        return; //already initialized
    TSLinkedListNode *dummy = TSLinkedListNodeAllocator::inst.get();
    head_.store(dummy);
    tail_.store(dummy);
}
void TSLinkedList::destroy()
{
    if (head_.load() != nullptr) {
        EpochBasedReclamation::inst.enter_epoch();
        //use EBR to release the dummy node
        for (TSLinkedListNode *cur = head_.load(); cur != nullptr; cur = cur->next_.load()) {
            EpochBasedReclamation::inst.retire(cur);
        }
        head_.store(nullptr);
        tail_.store(nullptr);
        EpochBasedReclamation::inst.exit_epoch();
    }
}
void *TSLinkedList::pop_front()
{
    init();
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNode *first = head_.load();
        TSLinkedListNode *last = tail_.load();
        assert(!first->released_.load());
        assert(!last->released_.load());
        TSLinkedListNode *next = first->next_.load();
        //assert(next == nullptr || !next->released.load());
        if (first == last) {
            if (next == nullptr) { // queue is empty
                EpochBasedReclamation::inst.exit_epoch();
                return nullptr;
            }
            tail_.compare_exchange_weak(last, next); // tail is falling behind. update tail
        } else if (next != nullptr) {
            void *value = next->value_;
            if (head_.compare_exchange_strong(first, next)) {
                assert(first != next); //make sure not self-loop
                EpochBasedReclamation::inst.retire(first);
                EpochBasedReclamation::inst.exit_epoch();
                return value;
            }
        }
        EpochBasedReclamation::inst.exit_epoch();
    }
}
void TSLinkedList::push_back(void *value)
{
    init();
    TSLinkedListNode *node = TSLinkedListNodeAllocator::inst.get();
    node->value_ = value;
    node->next_.store(nullptr);
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNode *last = tail_.load();
        TSLinkedListNode *next = last->next_.load();
        if (next == nullptr) {
            if (last->next_.compare_exchange_weak(next, node)) {
                assert(last != node); //make sure not self-loop
                tail_.compare_exchange_weak(last, node);
                EpochBasedReclamation::inst.exit_epoch();
                break;
            }
        } else
            tail_.compare_exchange_weak(last, next);
        EpochBasedReclamation::inst.exit_epoch();
    }
}

void TSLinkedListNodeAllocator::init()
{
}

std::atomic<uint64_t> new_count{0};
void TSLinkedListNodeAllocator::destroy()
{
    for (int i = tl_node_cache_count_.load() - 1; i >= 0; --i) {
        tl_node_cache_[i].destroy();
    }
}

void TSLinkedListNodeAllocator::init_thread_local_cache()
{
    if (ts_node_cache_index >= 0)
        return; //already initialized
    ts_node_cache_index = tl_node_cache_count_.fetch_add(1);
    assert(ts_node_cache_index < TS_LINKED_LIST_NODE_CACHE_MAX_THREADS);
}

TSLinkedListNode *TSLinkedListNodeAllocator::get()
{
    if (ts_node_cache_index < 0) { //thread local cache not initialized
        TSLinkedListNodeAllocator::inst.init_thread_local_cache();
    }
    TSLinkedListNodeCache &cache = tl_node_cache_[ts_node_cache_index];
    TSLinkedListNode *node = cache.pop();
    if (node == nullptr) {
        node = new TSLinkedListNode(nullptr);
        new_count.fetch_add(1);
    }
    node->released_.store(false);
    node->next_.store(nullptr);
    return node;
}

void TSLinkedListNodeAllocator::release(TSLinkedListNode *node)
{
    init_thread_local_cache();
    TSLinkedListNodeCache &cache = tl_node_cache_[ts_node_cache_index];
    node->released_.store(true);
    cache.push(node);
    if (cache.count_ >= TS_LINKED_LIST_NODE_CACHE_COUNT) {
        batch_release();
    }
}

void TSLinkedListNodeAllocator::batch_release()
{
    init_thread_local_cache();
    TSLinkedListNodeCache &cache = tl_node_cache_[ts_node_cache_index];
    for (int i = 0; i < TS_LINKED_LIST_NODE_CACHE_BATCH_RELEASE_COUNT; ++i) {
        TSLinkedListNode *node = cache.pop();
        if (node == nullptr)
            break;
        delete node;
    }
}

uint64_t TSLinkedListNodeAllocator::get_new_count()
{
    return new_count.load();
}

void TSLinkedListNodeCache::destroy()
{
    while (head_ != nullptr) {
        TSLinkedListNode *node = head_;
        head_ = head_->next_.load();
        delete node;
    }
}

void TSLinkedListNodeCache::push(TSLinkedListNode *node)
{
    node->next_.store(head_);
    head_ = node;
    count_++;
}

TSLinkedListNode *TSLinkedListNodeCache::pop()
{
    if (head_ == nullptr) {
        return nullptr;
    }
    TSLinkedListNode *ret = head_;
    head_ = head_->next_.load();
    count_--;
    return ret;
}
} // namespace rockcoro