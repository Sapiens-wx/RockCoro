#pragma once
#include <cstddef>
#include "config.h"

namespace rockcoro {

struct Stack {
    // a pointer to the allocated memory. Represents stack top.
    char *stack_mem;
    // size of the buffer in bytes
    size_t size;

    Stack();
    ~Stack();
};

} // namespace rockcoro