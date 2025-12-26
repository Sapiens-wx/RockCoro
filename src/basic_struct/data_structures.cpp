#include "data_structures.h"
#include "coroutine/coroutine.h"

namespace rockcoro {

TSLinkedListNode::TSLinkedListNode(void *value)
    : value(value)
{
    next.store(nullptr, std::memory_order_relaxed);
}

TSLinkedListNode *TSLinkedList::pop_front()
{
    while (true) {
        TSLinkedListNode *first = head.load();
        if (first == nullptr)
            return nullptr;
        TSLinkedListNode *last = tail.load(std::memory_order_acquire);
        TSLinkedListNode *second = first->next.load();
        // if after we load [first] and [last], an element was pushed to the list, there are two options:
        // 1) try again, OR
        // 2) update tail here (set tail=second)
        if (first == last && second != nullptr)
            continue;
        if (head.compare_exchange_weak(first, second)) {
            first->next.compare_exchange_strong(second, nullptr);
            // we are popping the last element, so the list should be empty. so set the tail to nullptr
            if (second == nullptr) {
                // here we might fail to update tail. Consider this: before executing the next line,
                // some thread has pushed an element to the list. Then head==nullptr but tail!=nullptr.
                // we need to set head to last->next, which is the newly pushed element
                if (!tail.compare_exchange_strong(last, nullptr))
                    head.compare_exchange_strong(second, last->next);
            }
            return first;
        }
    }
}
void TSLinkedList::push_back(TSLinkedListNode *value)
{
    TSLinkedListNode *self = value;
    self->next.store(nullptr);

    while (true) {
        TSLinkedListNode *first = head.load(std::memory_order_acquire);
        TSLinkedListNode *last = tail.load(std::memory_order_acquire);
        if (last == nullptr) { // the list is empty
            if (tail.compare_exchange_weak(last, self)) {
                head.compare_exchange_strong(first, self);
                return;
            }
        } else { // the list is not empty
            TSLinkedListNode *next = last->next.load(std::memory_order_acquire);

            if (next == nullptr) { // other threads haven't pushed
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
