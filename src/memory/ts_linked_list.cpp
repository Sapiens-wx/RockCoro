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
    if (head_.load().get_ptr() != nullptr)
        return; //already initialized
    TSLinkedListNodePtr dummy = TSLinkedListNodeAllocator::inst.get();
    head_.store(dummy);
    tail_.store(dummy);
}
void TSLinkedList::destroy()
{
    if (head_.load().get_ptr() != nullptr) {
        EpochBasedReclamation::inst.enter_epoch();
        //use EBR to release the dummy node
        for (TSLinkedListNodePtr cur = head_.load(); cur.get_ptr() != nullptr;
             cur = cur->next_.load()) {
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
        TSLinkedListNodePtr first = head_.load();
        TSLinkedListNodePtr last = tail_.load();
        assert(!first->released_.load());
        assert(!last->released_.load());
        TSLinkedListNodePtr next = first->next_.load();
        //assert(next == nullptr || !next->released.load());
        if (first.get_ptr() == last.get_ptr()) {
            if (next.get_ptr() == nullptr) { // queue is empty
                EpochBasedReclamation::inst.exit_epoch();
                return nullptr;
            }
            tail_.compare_exchange_weak(last, next); // tail is falling behind. update tail
        } else if (next.get_ptr() != nullptr) {
            void *value = next->value_;
            if (head_.compare_exchange_strong(first, next)) {
                assert(first.get_ptr() != next.get_ptr()); //make sure not self-loop
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
    TSLinkedListNodePtr node = TSLinkedListNodeAllocator::inst.get();
    node.increment_tag();
    node->value_ = value;
    node->next_.store(nullptr);
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNodePtr last = tail_.load();
        TSLinkedListNodePtr next = last->next_.load();
        if (next.get_ptr() == nullptr) {
            if (last->next_.compare_exchange_weak(next, node)) {
                assert(last.get_ptr() != node.get_ptr()); //make sure not self-loop
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

TSLinkedListNodePtr TSLinkedListNodeAllocator::get()
{
    if (ts_node_cache_index < 0) { //thread local cache not initialized
        TSLinkedListNodeAllocator::inst.init_thread_local_cache();
    }
    TSLinkedListNodeCache &cache = tl_node_cache_[ts_node_cache_index];
    TSLinkedListNodePtr node = cache.pop();
    if (node.get_ptr() == nullptr) {
        node = new TSLinkedListNode(nullptr);
        new_count.fetch_add(1);
    }
    node->released_.store(false);
    node->next_.store(nullptr);
    return node;
}

void TSLinkedListNodeAllocator::release(TSLinkedListNodePtr node)
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
        TSLinkedListNodePtr node = cache.pop();
        if (node.get_ptr() == nullptr)
            break;
        delete node.get_ptr();
    }
}

uint64_t TSLinkedListNodeAllocator::get_new_count()
{
    return new_count.load();
}

void TSLinkedListNodeCache::destroy()
{
    while (head_.get_ptr() != nullptr) {
        TSLinkedListNodePtr node = head_;
        head_ = head_->next_.load();
        delete node.get_ptr();
    }
}

void TSLinkedListNodeCache::push(TSLinkedListNodePtr node)
{
    node->next_.store(head_);
    head_ = node;
    count_++;
}

TSLinkedListNodePtr TSLinkedListNodeCache::pop()
{
    if (head_.get_ptr() == nullptr) {
        return nullptr;
    }
    TSLinkedListNodePtr ret = head_;
    head_ = head_->next_.load();
    count_--;
    return ret;
}
} // namespace rockcoro