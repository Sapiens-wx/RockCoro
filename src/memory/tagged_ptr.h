#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace rockcoro {

constexpr uint64_t TAGGED_PTR_MASK = 0b111;

template <typename T> struct TaggedPtr {
    T *ptr_ = nullptr;
    TaggedPtr()
    {
    }
    TaggedPtr(T *ptr)
        : ptr_(ptr)
    {
    }
    TaggedPtr(T *ptr, int tag)
    {
        ptr_ = (T *)(((uint64_t)ptr) | (tag & TAGGED_PTR_MASK));
    }
    T *get_ptr()
    {
        return ((uint64_t)ptr_) & ~TAGGED_PTR_MASK;
    }
    int get_tag()
    {
        return ((uint64_t)ptr_) & TAGGED_PTR_MASK;
    }
    T *operator->()
    {
        return get_ptr();
    }
};
} // namespace rockcoro