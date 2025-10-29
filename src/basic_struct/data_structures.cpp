#include "data_structures.h"
#include "coroutine/coroutine.h"

namespace rockcoro
{

    LinkedListNode::LinkedListNode(Coroutine *coroutine)
        : next(nullptr), coroutine(coroutine)
    {
    }

    LinkedList::LinkedList()
        : head(nullptr), tail(nullptr)
    {
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
        if (head == nullptr)
        {
            head = &coroutine->node;
            tail = head;
        }
        else
        {
            tail->next = &coroutine->node;
            tail = tail->next;
        }
        tail->next = nullptr;
    }

}
