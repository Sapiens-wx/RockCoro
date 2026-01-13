#include "memory/ts_linked_list.h"
#include <cstdlib>
#include <thread>
#include "memory/epoch_based_reclamation.h"

namespace rockcoro {
/*static void chaos_yield()
{
    if (rand() % 7 == 0)
        std::this_thread::yield();
}*/
TSLinkedListNode::TSLinkedListNode(void *value)
    : value(value)
{
    next.store(nullptr, std::memory_order_relaxed);
}

TSLinkedList::TSLinkedList()
{
    TSLinkedListNode *dummy = TSLinkedListNodeAllocator::inst.get();
    head.store(dummy);
    tail.store(dummy);
}
TSLinkedList::~TSLinkedList()
{
    destroy();
}
void TSLinkedList::destroy()
{
    if (head.load() != nullptr) {
        EpochBasedReclamation::inst.enter_epoch();
        //use EBR to release the dummy node
        for (TSLinkedListNode *cur = head.load(); cur != nullptr; cur = cur->next.load()) {
            cur->used_by_thread_epoch.store(-1);
            EpochBasedReclamation::inst.retire(cur);
        }
        head.store(nullptr);
        tail.store(nullptr);
        EpochBasedReclamation::inst.exit_epoch();
    }
}
void *TSLinkedList::pop_front()
{
    while (true) {
        EpochBasedReclamation::inst.enter_epoch();
        TSLinkedListNode *first = head.load();
        TSLinkedListNode *last = tail.load();
        //assert(!first->released.load());
        //assert(!last->released.load());
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
                first->used_by_thread_epoch.store(-1);
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
    TSLinkedListNode *node = TSLinkedListNodeAllocator::inst.get();
    node->value = value;
    node->next.store(nullptr);
    node->used_by_thread_epoch.store(EpochBasedReclamation::inst.global_epoch.load());
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

void TSLinkedListNodeAllocator::destroy()
{
    TSLinkedListNode *cur = head.load();
    head.store(nullptr);
    for (; cur != nullptr;) {
        TSLinkedListNode *next = cur->next.load();
        delete cur;
        cur = next;
    }
}

TSLinkedListNode *TSLinkedListNodeAllocator::get()
{
    TSLinkedListNode *first = head.load();
    do {
        if (first == nullptr) {
            TSLinkedListNode *ret = new TSLinkedListNode(nullptr);
            return ret;
        }
        //if (first == nullptr)
        //return new TSLinkedListNode(nullptr);
    } while (!head.compare_exchange_weak(first, first->next.load()));
    first->next.store(nullptr);
    first->released.store(false);
    return first;
}

void TSLinkedListNodeAllocator::release(TSLinkedListNode *node)
{
    node->released.store(true);
    TSLinkedListNode *first = head.load();
    do {
        node->next.store(first);
        assert(node != first); //make sure not self-loop
    } while (!head.compare_exchange_weak(first, node));
}

} // namespace rockcoro