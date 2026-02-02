#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace rockcoro {

template <typename T, size_t SEG_SIZE = 1024, size_t SEG_COUNT = 1024> struct Queue {
    struct Segment {
        T buffer[SEG_SIZE];
    };

    Segment *data[SEG_COUNT];
    size_t top = 0, bottom = 0; // bottom is exclusive

    Queue()
    {
        for (size_t i = 0; i < SEG_COUNT; ++i) {
            data[i] = nullptr;
        }
    }

    ~Queue()
    {
        for (size_t i = 0; i < SEG_COUNT; ++i) {
            if (data[i])
                delete data[i];
        }
    }

    Segment *segment(size_t idx)
    {
        return data[idx / SEG_SIZE];
    }
    size_t offset(size_t idx)
    {
        return idx % SEG_SIZE;
    }

    void push(const T &item)
    {
        size_t b_next = bottom + 1;

        // if bottom exceeds the bound, wrap-around
        if (b_next >= SEG_SIZE * SEG_COUNT) {
            b_next = 0;
        }
        // if assert fails, then the deque is out of capacity
        assert(top != b_next);

        size_t seg_idx = bottom / SEG_SIZE;
        if (!data[seg_idx]) {
            data[seg_idx] = new Segment();
        }

        data[seg_idx]->buffer[offset(bottom)] = item;

        bottom = b_next;
    }

    /// @brief pop an item from the front.
    /// @return nullptr if the queue is empty
    T *pop()
    {
        T *item = nullptr;

        if (top != bottom) { // deque is not empty
            item = &segment(top)->buffer[offset(top)];
            top = top + 1;
            if (top == SEG_SIZE * SEG_COUNT) {
                top = 0;
            }
        }
        return item;
    }
};
} // namespace rockcoro