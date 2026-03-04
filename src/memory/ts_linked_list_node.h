#pragma once
#include <atomic>
#include "memory/tagged_ptr.h"

namespace rockcoro {

struct TSLinkedListNode;
using TSLinkedListNodePtr = TaggedPtr<TSLinkedListNode>;

//thread safe linked list node
struct TSLinkedListNode {
    std::atomic<TSLinkedListNodePtr> next_;
    void *value_;
    //debug
    std::atomic<bool> released_ = false;

    inline TSLinkedListNode(void *value);
};

inline TSLinkedListNode::TSLinkedListNode(void *value)
    : value_(value)
{
    next_.store(nullptr, std::memory_order_relaxed);
}

} // namespace rockcoro