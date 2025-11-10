#include "stack.h"

namespace rockcoro {

Stack::Stack()
{
    stack_mem = new StackMem();
}

Stack::~Stack()
{
    delete stack_mem;
}

} // namespace rockcoro