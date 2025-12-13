#include "stack.h"

namespace rockcoro {

void Stack::init()
{
    stack_mem = new StackMem();
}

Stack::~Stack()
{
    delete stack_mem;
}

} // namespace rockcoro