#pragma once
#include <cstddef>
#include "config.h"

namespace rockcoro {

struct Stack {
    // a pointer to the allocated memory. Represents stack top.
    char *stack_mem_;
    // size of the buffer in bytes
    size_t size_;

    Stack();
    ~Stack();
};

} // namespace rockcoro