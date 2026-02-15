#pragma once
#include <cassert>
#include "log.h"
namespace rockcoro {

template <typename T, size_t INITIAL_CAPACITY = 64> class Deque {
public:
    Deque()
        : data_(allocate(INITIAL_CAPACITY))
        , size_(0)
        , capacity_(INITIAL_CAPACITY)
        , head_(0)
    {
    }

    ~Deque()
    {
        destroy_all();
        operator delete(data_);
    }

    Deque(const Deque &) = delete;
    Deque &operator=(const Deque &) = delete;

    // ---------- 容量 ----------
    size_t size() const
    {
        return size_;
    }
    bool empty() const
    {
        return size_ == 0;
    }

    // ---------- 操作 ----------
    void push_back(const T &value)
    {
        ensure_capacity(size_ + 1);
        size_t idx = physical_index(size_);
        new (data_ + idx) T(value);
        ++size_;
    }

    void push_back(T &&value)
    {
        ensure_capacity(size_ + 1);
        size_t idx = physical_index(size_);
        new (data_ + idx) T(std::move(value));
        ++size_;
    }

    void pop_front()
    {
        assert(size_ > 0);
        data_[head_].~T();
        head_ = (head_ + 1) % capacity_;
        --size_;
    }

    T &front()
    {
        assert(size_ > 0);
        return data_[head_];
    }

    const T &front() const
    {
        assert(size_ > 0);
        return data_[head_];
    }

private:
    T *data_;
    size_t size_;
    size_t capacity_;
    size_t head_;

    static T *allocate(size_t n)
    {
        if (n == 0)
            return nullptr;
        return static_cast<T *>(operator new(sizeof(T) * n));
    }

    size_t physical_index(size_t logical_index) const
    {
        return (head_ + logical_index) % capacity_;
    }

    void destroy_all()
    {
        for (size_t i = 0; i < size_; ++i) {
            size_t idx = physical_index(i);
            data_[idx].~T();
        }
    }

    void ensure_capacity(size_t min_cap)
    {
        if (min_cap <= capacity_)
            return;

        size_t new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
        if (new_cap < min_cap)
            new_cap = min_cap;

        T *new_data = allocate(new_cap);

        // 按逻辑顺序搬迁
        for (size_t i = 0; i < size_; ++i) {
            size_t old_idx = physical_index(i);
            new (new_data + i) T(std::move(data_[old_idx]));
        }

        destroy_all();
        operator delete(data_);

        data_ = new_data;
        capacity_ = new_cap;
        head_ = 0;
    }
};
} // namespace rockcoro