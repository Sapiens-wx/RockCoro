#include "stack.h"
#include <sys/mman.h>
#include <unistd.h>

namespace rockcoro {

constexpr size_t STACK_SIZE = 1024 * 1024;

Stack::Stack()
{
    size_t page_size = sysconf(_SC_PAGESIZE);
    // make sure stack_size is a multiple of page_size
    size_t stack_size = (STACK_SIZE + page_size - 1) & ~(page_size - 1);
    size_t total_size = page_size + stack_size;

    void *base =
        mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (base == MAP_FAILED)
        throw "stack mem buffer allocation failed";

    //this prevents the stack from overflow
    mprotect(base, page_size, PROT_NONE);

    stack_mem = ((char *)base) + page_size; // actual start of usable memory
    size = stack_size;
}

Stack::~Stack()
{
    size_t page_size = sysconf(_SC_PAGESIZE);
    // memory: | Guarded Page | Stack Mem ---- |
    munmap(stack_mem - page_size, page_size + size);
}

} // namespace rockcoro