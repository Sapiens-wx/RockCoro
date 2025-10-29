#pragma once
#include <cstddef>

namespace rockcoro
{

#define STACK_SIZE 1024 * 1024

    // a fix-sized buffer representing stack memory
    struct StackMem
    {
        // a pointer to the allocated memory. Represents stack top.
        char buffer[STACK_SIZE];
        // size of the buffer in bytes
        size_t size = STACK_SIZE;
    };

    struct Stack
    {
        StackMem *stack_mem;

        Stack();
        ~Stack();
    };

}