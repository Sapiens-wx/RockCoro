#pragma once
#include <vector>
#include "basic_struct/chunked_vector.h"

namespace rockcoro {

// non-shrinking object pool
template <typename T, size_t BlockSize = 1024 * sizeof(T)> class ObjectPool {
public:
    explicit ObjectPool(size_t capacity)
    {
        constexpr size_t count_per_block = BlockSize / sizeof(T);
        const size_t num_blocks_needed = (capacity + count_per_block - 1) / count_per_block;

        blocks_.resize(num_blocks_needed);
        free_list_.reserve(num_blocks_needed * count_per_block);
        for (size_t i = 0; i < num_blocks_needed; ++i) {
            blocks_[i] = new Block();
            for (size_t j = 0; j < count_per_block; ++j) {
                free_list_.push_back(&(blocks_[i]->data[j]));
            }
        }
    }
    ~ObjectPool()
    {
        for (Block *block : blocks_) {
            delete block;
        }
    }

    T *acquire()
    {
        if (free_list_.empty()) {
            grow();
        }
        T *ret = free_list_.back();
        free_list_.pop_back();
        return ret;
    }
    void release(T *ptr)
    {
        free_list_.push_back(ptr);
    }

    size_t capacity() const
    {
        constexpr size_t count_per_block = BlockSize / sizeof(T);
        return blocks_.size() * count_per_block;
    }

private:
    struct Block {
        T data[BlockSize / sizeof(T)];
    };

    std::vector<Block *> blocks_;
    std::vector<T *> free_list_;

    void grow()
    {
        constexpr size_t count_per_block = BlockSize / sizeof(T);
        Block *block = new Block();
        blocks_.push_back(block);
        free_list_.reserve(count_per_block);
        for (size_t i = 0; i < count_per_block; ++i) {
            free_list_.push_back(&block->data[i]);
        }
    }
};

} // namespace rockcoro