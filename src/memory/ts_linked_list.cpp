#include "memory/ts_linked_list.h"
#include <cassert>
#include <cstdlib>
#include <thread>
#include "memory/epoch_based_reclamation.h"

namespace rockcoro {

static thread_local int ts_node_cache_index = -1;

TSLinkedListNode::TSLinkedListNode(void *value)
    : value(value)
{
    next.store(nullptr, std::memory_order_relaxed);
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
    if (head.load() != nullptr)
        return; //already initialized
    TSLinkedListNode *dummy = TSLinkedListNodeAllocator::inst.get();
    head.store(dummy);
    tail.store(dummy);
}
void TSLinkedList::destroy()
{
    if (head.load() != nullptr) {
        EpochBasedReclamation::inst.enter_epoch();
        //use EBR to release the dummy node
        for (TSLinkedListNode *cur = head.load(); cur != nullptr; cur = cur->next.load()) {
            EpochBasedReclamation::inst.retire(cur);
        }
        head.store(nullptr);
        tail.store(nullptr);
        EpochBasedReclamation::inst.exit_epoch();
    }
}
void *TSLinkedList::pop_front()
{
#ifndef NDEBUG
    assert(head.load() != nullptr); // not initialized
#else
    init();
#endif
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNode *first = head.load();
        TSLinkedListNode *last = tail.load();
        assert(!first->released.load());
        assert(!last->released.load());
        TSLinkedListNode *next = first->next.load();
        //assert(next == nullptr || !next->released.load());
        if (first == last) {
            if (next == nullptr) { // queue is empty
                EpochBasedReclamation::inst.exit_epoch();
                return nullptr;
            }
            tail.compare_exchange_weak(last, next); // tail is falling behind. update tail
        } else if (next != nullptr) {
            void *value = next->value;
            if (head.compare_exchange_strong(first, next)) {
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
#ifndef NDEBUG
    assert(head.load() != nullptr); // not initialized
#else
    init();
#endif
    TSLinkedListNode *node = TSLinkedListNodeAllocator::inst.get();
    node->value = value;
    node->next.store(nullptr);
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNode *last = tail.load();
        TSLinkedListNode *next = last->next.load();
        if (next == nullptr) {
            if (last->next.compare_exchange_weak(next, node)) {
                assert(last != node); //make sure not self-loop
                tail.compare_exchange_weak(last, node);
                EpochBasedReclamation::inst.exit_epoch();
                break;
            }
        } else
            tail.compare_exchange_weak(last, next);
        EpochBasedReclamation::inst.exit_epoch();
    }
}

void TSLinkedListNodeAllocator::init()
{
}

std::atomic<uint64_t> new_count{0};
void TSLinkedListNodeAllocator::destroy()
{
    for (int i = tl_node_cache_count.load() - 1; i >= 0; --i) {
        tl_node_cache[i].destroy();
    }
}

void TSLinkedListNodeAllocator::init_thread_local_cache()
{
    if (ts_node_cache_index >= 0)
        return; //already initialized
    ts_node_cache_index = tl_node_cache_count.fetch_add(1);
    assert(ts_node_cache_index < TS_LINKED_LIST_NODE_CACHE_MAX_THREADS);
}

TSLinkedListNode *TSLinkedListNodeAllocator::get()
{
#ifndef NDEBUG
    assert(ts_node_cache_index >= 0); //thread local cache not initialized
#else
    if (ts_node_cache_index < 0) {
        TSLinkedListNodeCache::inst.init_thread_local_cache();
    }
#endif
    TSLinkedListNodeCache &cache = tl_node_cache[ts_node_cache_index];
    TSLinkedListNode *node = cache.pop();
    if (node == nullptr) {
        node = new TSLinkedListNode(nullptr);
        new_count.fetch_add(1);
    }
    node->released.store(false);
    node->next.store(nullptr);
    return node;
}

void TSLinkedListNodeAllocator::release(TSLinkedListNode *node)
{
    assert(ts_node_cache_index >= 0); //thread local cache not initialized
    TSLinkedListNodeCache &cache = tl_node_cache[ts_node_cache_index];
    node->released.store(true);
    cache.push(node);
    if (cache.count >= TS_LINKED_LIST_NODE_CACHE_COUNT) {
        batch_release();
    }
}

void TSLinkedListNodeAllocator::batch_release()
{
    assert(ts_node_cache_index >= 0); //thread local cache not initialized
    TSLinkedListNodeCache &cache = tl_node_cache[ts_node_cache_index];
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
    while (head != nullptr) {
        TSLinkedListNode *node = head;
        head = head->next.load();
        delete node;
    }
}

void TSLinkedListNodeCache::push(TSLinkedListNode *node)
{
    node->next.store(head);
    head = node;
    count++;
}

TSLinkedListNode *TSLinkedListNodeCache::pop()
{
    if (head == nullptr) {
        return nullptr;
    }
    TSLinkedListNode *ret = head;
    head = head->next.load();
    count--;
    return ret;
}
} // namespace rockcoro