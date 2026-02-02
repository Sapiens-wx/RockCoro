#pragma once
namespace rockcoro {

//linked list node
template <typename T> struct LinkedListNode {
    LinkedListNode<T> *next;
    T *value;

    LinkedListNode(T *value);
};

//linked list. can be used only by scheduler
template <typename T> struct LinkedList {
    LinkedListNode<T> *head = nullptr;
    LinkedListNode<T> *tail = nullptr;

    // pop an element from head. returns nullptr if empty
    LinkedListNode<T> *pop_front();
    void push_front(LinkedListNode<T> *node);
    void push_back(LinkedListNode<T> *node);
};

template <typename T>
LinkedListNode<T>::LinkedListNode(T *value)
    : value(value)
    , next(nullptr)
{
}

template <typename T> LinkedListNode<T> *LinkedList<T>::pop_front()
{
    if (head == nullptr)
        return nullptr;
    LinkedListNode<T> *first = head;
    head = head->next;
    if (head == nullptr)
        tail = nullptr;
    first->next = nullptr;
    return first;
}

template <typename T> void LinkedList<T>::push_back(LinkedListNode<T> *node)
{
    node->next = nullptr;
    if (tail == nullptr) {
        head = node;
        tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
}

template <typename T> void LinkedList<T>::push_front(LinkedListNode<T> *node)
{
    node->next = head;
    head = node;
    if (tail == nullptr)
        tail = head;
}

} // namespace rockcoro