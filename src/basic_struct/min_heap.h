#pragma once
#include <queue>
#include <utility>
#include "basic_struct/object_pool.h"

namespace rockcoro {

// stores T*
// assumes that T has member variable heap_index_
template <typename T, typename Compare, typename Container = std::vector<T *>> class IntrusiveHeap {
public:
    IntrusiveHeap() = default;

    bool empty() const
    {
        return data_.empty();
    }
    size_t size() const
    {
        return data_.size();
    }
    void reserve(size_t capacity)
    {
        data_.reserve(capacity);
    }

    T *top() const
    {
        assert(!data_.empty());
        return data_[0];
    }

    void push(T *x)
    {
        assert(x->heap_index_ == -1); // 不能重复插入
        int idx = (int)data_.size();
        data_.push_back(x);
        x->heap_index_ = idx;
        sift_up(idx);
    }

    // 弹出最小
    T *pop()
    {
        assert(!data_.empty());
        T *ret = data_[0];
        remove_at(0);
        return ret;
    }

    // update the value of a node and shift up/down
    void update(T *x)
    {
        int idx = x->heap_index_;
        assert(idx != -1);

        if (comp_(x, data_[parent(idx)])) {
            sift_up(idx);
        } else {
            sift_down(idx);
        }
    }

private:
    Container data_;
    Compare comp_; // comp(a, b) = a < b（min-heap）

    static int parent(int i)
    {
        return (i - 1) / 2;
    }
    static int left(int i)
    {
        return i * 2 + 1;
    }
    static int right(int i)
    {
        return i * 2 + 2;
    }

    void swap_node(int i, int j)
    {
        std::swap(data_[i], data_[j]);
        data_[i]->heap_index_ = i;
        data_[j]->heap_index_ = j;
    }

    void sift_up(int i)
    {
        while (i > 0) {
            int p = parent(i);
            if (!comp_(data_[i], data_[p]))
                break;
            swap_node(i, p);
            i = p;
        }
    }

    void sift_down(int i)
    {
        int n = (int)data_.size();
        while (true) {
            int l = left(i);
            int r = right(i);
            int smallest = i;

            if (l < n && comp_(data_[l], data_[smallest])) {
                smallest = l;
            }
            if (r < n && comp_(data_[r], data_[smallest])) {
                smallest = r;
            }
            if (smallest == i)
                break;

            swap_node(i, smallest);
            i = smallest;
        }
    }

    void remove_at(int idx)
    {
        int last = (int)data_.size() - 1;
        T *removed = data_[idx];

        if (idx != last) {
            swap_node(idx, last);
        }

        data_.pop_back();
        removed->heap_index_ = -1;

        if (idx < (int)data_.size()) {
            // 关键：不知道该上还是下 → 两边试
            sift_down(idx);
            sift_up(idx);
        }
    }
};

// automatically manages memory of T using ObjectPool
// assumes that T has member variable heap_index_
template <typename T, typename Compare, size_t BlockSize = 1024 * sizeof(T)> class MinHeap {
public:
    explicit MinHeap(size_t capacity, Compare comp = Compare())
        : pool_(capacity)
    {
    }

    template <typename... Args> T *emplace(Args &&...args)
    {
        T *obj = pool_.acquire();
        new (obj) T(std::forward<Args>(args)...);
        obj->heap_index_ = -1;
        heap_.push(obj);
        return obj;
    }

    void pop()
    {
        pool_.release(heap_.pop());
    }

    T *top()
    {
        return heap_.top();
    }

    void update(T *node)
    {
        heap_.update(node);
    }

    bool empty() const
    {
        return heap_.empty();
    }
    size_t size() const
    {
        return heap_.size();
    }

private:
    ObjectPool<T, BlockSize> pool_;
    IntrusiveHeap<T, Compare> heap_;
};

} // namespace rockcoro