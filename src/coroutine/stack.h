#pragma once
#include <cstddef>
#include "config.h"

namespace rockcoro {

// a fix-sized buffer representing stack memory
struct StackMem {
    // a pointer to the allocated memory. Represents stack top.
    char buffer[STACK_SIZE];
    // size of the buffer in bytes
    size_t size = STACK_SIZE;
};

struct Stack {
    StackMem *stack_mem = nullptr;

    // allocate stack memory
    void init();
    ~Stack();
};

} // namespace rockcoro