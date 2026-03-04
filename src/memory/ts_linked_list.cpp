#include "memory/ts_linked_list.h"
#include <cassert>
#include <cstdlib>
#include <thread>
#include "memory/epoch_based_reclamation.h"


namespace rockcoro {

static thread_local int ts_node_cache_index = -1;

void TSLinkedListNodeAllocator::init()
{
}

std::atomic<uint64_t> new_count{0};
void TSLinkedListNodeAllocator::destroy()
{
    for (int i = tl_node_cache_count_.load(std::memory_order_relaxed) - 1; i >= 0; --i) {
        tl_node_cache_[i].destroy();
    }
}

void TSLinkedListNodeAllocator::init_thread_local_cache()
{
    if (ts_node_cache_index >= 0)
        return; //already initialized
    ts_node_cache_index = tl_node_cache_count_.fetch_add(1, std::memory_order_relaxed);
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
        new_count.fetch_add(1, std::memory_order_relaxed);
    }
    node->released_.store(false, std::memory_order_relaxed);
    node->next_.store(nullptr, std::memory_order_relaxed);
    return node;
}

void TSLinkedListNodeAllocator::release(TSLinkedListNodePtr node)
{
    init_thread_local_cache();
    TSLinkedListNodeCache &cache = tl_node_cache_[ts_node_cache_index];
    node->released_.store(true, std::memory_order_relaxed);
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
    return new_count.load(std::memory_order_relaxed);
}

void TSLinkedListNodeCache::destroy()
{
    while (head_.get_ptr() != nullptr) {
        TSLinkedListNodePtr node = head_;
        head_ = head_->next_.load(std::memory_order_relaxed);
        delete node.get_ptr();
    }
}

void TSLinkedListNodeCache::push(TSLinkedListNodePtr node)
{
    node->next_.store(head_, std::memory_order_relaxed);
    head_ = node;
    count_++;
}

TSLinkedListNodePtr TSLinkedListNodeCache::pop()
{
    if (head_.get_ptr() == nullptr) {
        return nullptr;
    }
    TSLinkedListNodePtr ret = head_;
    head_ = head_->next_.load(std::memory_order_relaxed);
    count_--;
    return ret;
}
} // namespace rockcoro