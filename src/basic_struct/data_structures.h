#pragma once
#include <assert.h>
#include <atomic>
#include <pthread.h>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

namespace rockcoro {

struct Coroutine;

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

//linked list node
template <typename T> struct LinkedListNode {
    LinkedListNode<T> *next;
    T *value;

    LinkedListNode(T *value);
};

//linked list. can be used only by scheduler
template <typename T> struct LinkedList {
    LinkedListNode<T> *head = nullptr;
    LinkedListNode<T> *tail = nullptr;

    // pop an element from head. returns nullptr if empty
    LinkedListNode<T> *pop_front();
    void push_front(LinkedListNode<T> *node);
    void push_back(LinkedListNode<T> *node);
};

/// @brief Chase-Lev Deque
/// @tparam T
/// @tparam SEG_SIZE max number of elements in this segment
/// @tparam SEG_COUNT max number of segment in this deque
template <typename T, size_t SEG_SIZE = 1024, size_t SEG_COUNT = 1024> struct CLDeque {
    struct Segment {
        T buffer[SEG_SIZE];
    };

    Segment *data[SEG_COUNT];
    std::atomic<size_t> top;
    size_t bottom; // exclusive

    CLDeque()
        : top(0)
        , bottom(0)
    {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            data[i] = nullptr;
    }

    ~CLDeque()
    {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            if (data[i])
                delete data[i];
    }

    Segment *segment(size_t idx)
    {
        return data[idx / SEG_SIZE];
    }
    size_t offset(size_t idx)
    {
        return idx % SEG_SIZE;
    }

    void push_back(const T &item)
    {
        size_t b = bottom;
        size_t b_next = b + 1;

        // if bottom exceeds the bound, wrap-around
        if (b_next >= SEG_SIZE * SEG_COUNT)
            b_next = 0;
        // if assert fails, then the deque is out of capacity
        size_t t = top.load();
        assert(t != b_next);

        size_t seg_idx = b / SEG_SIZE;
        if (!data[seg_idx]) {
            data[seg_idx] = new Segment();
        }

        data[seg_idx]->buffer[offset(b)] = item;
        atomic_thread_fence(std::memory_order_release);

        bottom = b_next;
    }

    /// @brief returns a pointer to an element
    /// @return NULL if fails to pop
    const T *pop_back()
    {
        size_t old_bottom = bottom;
        size_t b = old_bottom == 0 ? SEG_SIZE * SEG_COUNT : old_bottom;
        --b;
        size_t t = top.load();
        size_t t_next = t + 1;
        if (t_next >= SEG_SIZE * SEG_COUNT)
            t_next = 0;

        if (top.load() == old_bottom) { // deque is empty
            return nullptr;
        } else {                        // deque is not empty
            bottom = b;
            atomic_thread_fence(std::memory_order_seq_cst);
            T *item = &segment(b)->buffer[offset(b)];

            if (top.load() == b) { // is the last element
                if (top.compare_exchange_strong(
                        t, t_next, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                    bottom = old_bottom; // bottom has to be set after top is CASed.
                    return item;         // CAS success
                } else {
                    bottom = old_bottom;
                    return nullptr; // stolen by a theft
                }
            }
            return item;
        }
    }

    const T *pop_front()
    {
        size_t t = top.load();
        size_t b = bottom;

        if (t != b) { // deque is not empty
            T *item = &segment(t)->buffer[offset(t)];
            size_t t_next = t + 1;
            if (t_next == SEG_SIZE * SEG_COUNT)
                t_next = 0;
            if (top.compare_exchange_strong(
                    t, t_next, std::memory_order_seq_cst, std::memory_order_seq_cst))
                return item;    // CAS success
            else
                return nullptr; // CAS fail
        } else                  // deque is empty
            return nullptr;
    }
};

/// @brief Michael-Scott queue
template <typename T, size_t SEG_SIZE = 1024, size_t SEG_COUNT = 1024> struct MSQueue {
    struct Segment {
        T buffer[SEG_SIZE];
    };

    Segment *data[SEG_COUNT];
    size_t top, bottom; // bottom is exclusive
    pthread_mutex_t mutex;

    MSQueue()
        : top(0)
        , bottom(0)
    {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            data[i] = nullptr;
        pthread_mutex_init(&mutex, nullptr);
    }

    ~MSQueue()
    {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            if (data[i])
                delete data[i];
        pthread_mutex_destroy(&mutex);
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
        pthread_mutex_lock(&mutex);
        size_t b_next = bottom + 1;

        // if bottom exceeds the bound, wrap-around
        if (b_next >= SEG_SIZE * SEG_COUNT)
            b_next = 0;
        // if assert fails, then the deque is out of capacity
        assert(top != b_next);

        size_t seg_idx = bottom / SEG_SIZE;
        if (!data[seg_idx]) {
            data[seg_idx] = new Segment();
        }

        data[seg_idx]->buffer[offset(bottom)] = item;

        bottom = b_next;
        pthread_mutex_unlock(&mutex);
    }

    /// @brief pop an item from the front.
    /// @return nullptr if the queue is empty
    const T *pop()
    {
        pthread_mutex_lock(&mutex);

        T *item = nullptr;

        if (top != bottom) { // deque is not empty
            item = &segment(top)->buffer[offset(top)];
            top = top + 1;
            if (top == SEG_SIZE * SEG_COUNT)
                top = 0;
        }
        pthread_mutex_unlock(&mutex);
        return item;
    }
};

template <typename T>
LinkedListNode<T>::LinkedListNode(T *value)
    : value(value)
    , next(nullptr)
{
}

template <typename T> LinkedListNode<T> *LinkedList<T>::pop_front()
{
    if (head == nullptr)
        return nullptr;
    LinkedListNode<T> *first = head;
    head = head->next;
    if (head == nullptr)
        tail = nullptr;
    first->next = nullptr;
    return first;
}

template <typename T> void LinkedList<T>::push_back(LinkedListNode<T> *node)
{
    node->next = nullptr;
    if (tail == nullptr) {
        head = node;
        tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
}

template <typename T> void LinkedList<T>::push_front(LinkedListNode<T> *node)
{
    node->next = head;
    head = node;
    if (tail == nullptr)
        tail = head;
}

template <typename T, size_t CAPACITY = 64> class Vector {
public:
    Vector()
        : data_(allocate(CAPACITY))
        , size_(0)
        , capacity_(CAPACITY)
    {
    }

    explicit Vector(size_t n)
        : data_(allocate(n))
        , size_(n)
        , capacity_(n)
    {
        for (size_t i = 0; i < n; ++i)
            new (data_ + i) T();
    }

    ~Vector()
    {
        destroy_all();
        operator delete(data_);
    }

    Vector(const Vector &other)
        : data_(allocate(other.capacity_))
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);
    }

    Vector &operator=(const Vector &other)
    {
        if (this == &other)
            return *this;

        destroy_all();
        operator delete(data_);

        data_ = allocate(other.capacity_);
        size_ = other.size_;
        capacity_ = other.capacity_;

        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);

        return *this;
    }

    Vector(Vector &&other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    Vector &operator=(Vector &&other) noexcept
    {
        if (this == &other)
            return *this;

        destroy_all();
        operator delete(data_);

        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;

        return *this;
    }

    T &operator[](size_t i)
    {
        assert(i < size_);
        return data_[i];
    }

    const T &operator[](size_t i) const
    {
        assert(i < size_);
        return data_[i];
    }

    // ---------- 容量 ----------
    size_t size() const
    {
        return size_;
    }
    size_t capacity() const
    {
        return capacity_;
    }
    bool empty() const
    {
        return size_ == 0;
    }

    void push_back(const T &value)
    {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T &&value)
    {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(std::move(value));
        ++size_;
    }

    template <typename... Args> T &emplace_back(Args &&...args)
    {
        ensure_capacity(size_ + 1);
        new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back()
    {
        assert(size_ > 0);
        data_[--size_].~T();
    }

    void clear()
    {
        destroy_all();
        size_ = 0;
    }

private:
    T *data_;
    size_t size_;
    size_t capacity_;

    static T *allocate(size_t n)
    {
        if (n == 0)
            return nullptr;
        return static_cast<T *>(operator new(sizeof(T) * n));
    }

    void destroy_all()
    {
        for (size_t i = 0; i < size_; ++i)
            data_[i].~T();
    }

    void ensure_capacity(size_t min_cap)
    {
        if (min_cap <= capacity_)
            return;

        size_t new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
        if (new_cap < min_cap)
            new_cap = min_cap;

        T *new_data = allocate(new_cap);

        // 移动构造
        for (size_t i = 0; i < size_; ++i)
            new (new_data + i) T(std::move(data_[i]));

        destroy_all();
        operator delete(data_);

        data_ = new_data;
        capacity_ = new_cap;
    }
};

template <typename T, size_t CAPACITY> class Array {
public:
    Array()
        : size_(0)
    {
    }

    explicit Array(size_t n)
        : size_(n)
    {
        assert(n <= CAPACITY);
        for (size_t i = 0; i < n; ++i)
            new (data_ + i) T();
    }

    ~Array()
    {
        destroy_all();
    }

    Array(const Array &other)
        : size_(other.size_)
    {
        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);
    }

    Array &operator=(const Array &other)
    {
        if (this == &other)
            return *this;

        destroy_all();
        size_ = other.size_;

        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(other.data_[i]);

        return *this;
    }

    Array(Array &&other) noexcept
        : size_(other.size_)
    {
        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(std::move(other.data_[i]));

        other.destroy_all();
        other.size_ = 0;
    }

    Array &operator=(Array &&other) noexcept
    {
        if (this == &other)
            return *this;

        destroy_all();
        size_ = other.size_;

        for (size_t i = 0; i < size_; ++i)
            new (data_ + i) T(std::move(other.data_[i]));

        other.destroy_all();
        other.size_ = 0;

        return *this;
    }

    T &operator[](size_t i)
    {
        assert(i < size_);
        return data_[i];
    }

    const T &operator[](size_t i) const
    {
        assert(i < size_);
        return data_[i];
    }

    // ---------- 容量 ----------
    size_t size() const
    {
        return size_;
    }

    constexpr size_t capacity() const
    {
        return CAPACITY;
    }

    bool empty() const
    {
        return size_ == 0;
    }

    // ---------- 修改 ----------
    void push_back(const T &value)
    {
        assert(size_ < CAPACITY);
        new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T &&value)
    {
        assert(size_ < CAPACITY);
        new (data_ + size_) T(std::move(value));
        ++size_;
    }

    template <typename... Args> T &emplace_back(Args &&...args)
    {
        assert(size_ < CAPACITY);
        new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }

    void pop_back()
    {
        assert(size_ > 0);
        data_[--size_].~T();
    }

    void clear()
    {
        destroy_all();
        size_ = 0;
    }

private:
    alignas(T) unsigned char buffer_[sizeof(T) * CAPACITY];
    T *data_ = reinterpret_cast<T *>(buffer_);
    size_t size_;

    void destroy_all()
    {
        for (size_t i = 0; i < size_; ++i)
            data_[i].~T();
    }
};

} // namespace rockcoro
