#include "data_structures.h"
#include "coroutine/coroutine.h"

namespace rockcoro {

LinkedListNode::LinkedListNode(Coroutine *coroutine)
    : coroutine(coroutine)
{
    next.store(nullptr, std::memory_order_relaxed);
}

Coroutine *LinkedList::pop_front()
{
    while (true) {
        LinkedListNode *first = head.load();
        if (first == nullptr)
            return nullptr;
        LinkedListNode *last = tail.load(std::memory_order_acquire);
        LinkedListNode *second = first->next.load();
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
            return first->coroutine;
        }
    }
}
void LinkedList::push_back(Coroutine *coroutine)
{
    LinkedListNode *self = &coroutine->node;
    self->next.store(nullptr);

    while (true) {
        LinkedListNode *first = head.load(std::memory_order_acquire);
        LinkedListNode *last = tail.load(std::memory_order_acquire);
        if (last == nullptr) { // the list is empty
            if (tail.compare_exchange_weak(last, self)) {
                head.compare_exchange_strong(first, self);
                return;
            }
        } else { // the list is not empty
            LinkedListNode *next = last->next.load(std::memory_order_acquire);

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
