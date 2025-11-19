#include "data_structures.h"
#include "coroutine/coroutine.h"

namespace rockcoro {

LinkedListNode::LinkedListNode(Coroutine *coroutine)
    : coroutine(coroutine)
{
}

TLLinkedListNode::TLLinkedListNode(Coroutine *coroutine)
    : coroutine(coroutine)
{
    next.store(nullptr, std::memory_order_relaxed);
}

// pop an element from head. returns nullptr if empty
Coroutine *LinkedList::pop_front()
{
    if (head == nullptr)
        return nullptr;
    LinkedListNode *ret = head;
    head = head->next;
    if (head == nullptr)
        tail = nullptr;
    return ret->coroutine;
}
void LinkedList::push_back(Coroutine *coroutine)
{
    if (head == nullptr) {
        head = &coroutine->node;
        tail = head;
    } else {
        tail->next = &coroutine->node;
        tail = tail->next;
    }
    tail->next = nullptr;
}

Coroutine *TLLinkedList::pop_front()
{
    while (true) {
        TLLinkedListNode *first = head.load();
        TLLinkedListNode *last = tail.load();
        if (first == nullptr)
            return nullptr;
        TLLInkedListNode *second = first->next.load();
        if (head.compare_exchange_weak(first, second)) {
            first->next.compare_exchange_strong(second, nullptr);
            return first->coroutine;
        }
    }
}
void TLLinkedList::push_back(Coroutine *coroutine)
{
    TLLinkedListNode *self = &coroutine->timewheel_node;
    self->next.store(nullptr);

    while (true) {
        TLLinkedListNode *last = tail.load(std::memory_order_acquire);
        if (last == nullptr) { // the list is empty
            if (tail.compare_exchange_weak(last, self)) {
                head.compare_exchange_strong(head.load(), self);
                return;
            }
        } else { // the list is not empty
            TLLinkedListNode *next = last->next.load(std::memory_order_acquire);

            if (next == nullptr) { // other threads haven't updated pushed
                //try to append coroutine to the tail
                if (last->next.compare_exchange_weak(next, self)) {
                    tail.compare_exchange_strong(last, self);
                    return;
                }
            }
        }
    }
}

} // namespace rockcoro
