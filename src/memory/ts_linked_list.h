#pragma once
#include <atomic>

namespace rockcoro {

//thread safe linked list node
struct TSLinkedListNode {
    std::atomic<TSLinkedListNode *> next;
    void *value;
    std::atomic<bool> released = false;
    std::atomic<int> used_by_thread_epoch = -1;

    TSLinkedListNode(void *value);
};

struct TSLinkedListNodeAllocator {
    static TSLinkedListNodeAllocator inst;
    std::atomic<TSLinkedListNode *> head = nullptr;

    void init();
    void destroy();
    TSLinkedListNode *get();
    void release(TSLinkedListNode *node);
    //debug functions
    int get_new_count();
};

//thread safe linked list. can be used only by scheduler
// threads that uses TSLinkedList must call EpochBaseReclamation::inst.init_thread_epoch (so EBR could help avoid ABA problem)
struct TSLinkedList {
    std::atomic<TSLinkedListNode *> head = nullptr;
    std::atomic<TSLinkedListNode *> tail = nullptr;

    TSLinkedList();
    ~TSLinkedList();
    void destroy();
    // pop an element from head. returns nullptr if empty
    void *pop_front();
    void push_back(void *value);
};
} // namespace rockcoro