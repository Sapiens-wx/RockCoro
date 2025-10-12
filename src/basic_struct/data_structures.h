#ifndef ROCKCORO_DATA_STRUCTURES_H_
#define ROCKCORO_DATA_STRUCTURES_H_

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

namespace rockcoro{

struct CoroError{
    const char* msg;
    CoroError() = default;
    CoroError(const char* _msg):msg(_msg){}
};

template<typename T>
class Deque {
private:
    struct Node {
        T data;
        Node* prev;
        Node* next;
        Node(const T& val) : data(val), prev(nullptr), next(nullptr) {}
    };

    Node* head;
    Node* tail;
    size_t count;

public:
    Deque() : head(nullptr), tail(nullptr), count(0) {}

    ~Deque() {
        while (!empty()) pop_front();
    }

    inline bool empty() const { return count == 0; }
    inline size_t size() const { return count; }

    // 插入到头部
    void push_front(const T& val) {
        Node* node = new Node(val);
        node->next = head;
        if (head) head->prev = node;
        head = node;
        if (!tail) tail = head; // 第一个节点
        count++;
    }

    // 插入到尾部
    void push_back(const T& val) {
        Node* node = new Node(val);
        node->prev = tail;
        if (tail) tail->next = node;
        tail = node;
        if (!head) head = tail; // 第一个节点
        count++;
    }

    // 删除头部
    void pop_front() {
        if (empty()) throw CoroError("pop_front from empty deque");
        Node* tmp = head;
        head = head->next;
        if (head) head->prev = nullptr;
        else tail = nullptr;
        delete tmp;
        count--;
    }

    // 删除尾部
    void pop_back() {
        if (empty()) throw CoroError("pop_back from empty deque");
        Node* tmp = tail;
        tail = tail->prev;
        if (tail) tail->next = nullptr;
        else head = nullptr;
        delete tmp;
        count--;
    }

    T& front() {
        if (empty()) throw CoroError("front from empty deque");
        return head->data;
    }

    T& back() {
        if (empty()) throw CoroError("back from empty deque");
        return tail->data;
    }
};

/// @brief Chase-Lev Deque
/// @tparam T 
/// @tparam SEG_SIZE max number of elements in this segment
/// @tparam SEG_COUNT max number of segment in this deque
template<typename T, size_t SEG_SIZE = 1024, size_t SEG_COUNT = 1024>
struct CLDeque {
    struct Segment {
        T buffer[SEG_SIZE];
    };

    Segment* data[SEG_COUNT];
    _Atomic size_t top, count;
    size_t bottom; //exclusive

    CLDeque() : top(0), bottom(0), count(0) {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            data[i] = nullptr;
    }

    ~CLDeque() {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            if (data[i]) free(data[i]);
    }

    Segment* segment(size_t idx) { return data[idx / SEG_SIZE]; }
    size_t offset(size_t idx) { return idx % SEG_SIZE; }

    void push_back(const T& item) {
        size_t b = bottom;

        // if bottom exceeds the bound, wrap-around
        if (b >= SEG_SIZE * SEG_COUNT)
            b = 0;
        // if assert fails, then the deque is out of capacity
        size_t t = atomic_load_explicit(&top, memory_order_acquire);
        assert(t != b+1);

        size_t seg_idx = b / SEG_SIZE;
        if (!data[seg_idx]) {
            data[seg_idx] = new Segment();
        }

        data[seg_idx]->buffer[offset(b)] = item;
        atomic_thread_fence(memory_order_release);
        atomic_fetch_add(count, 1);
        bottom = b+1;
    }

    /// @brief returns a pointer to an element
    /// @return NULL if fails to pop
    const T* pop_back() {
        size_t old_bottom=bottom;
        size_t b = old_bottom==0?SEG_SIZE*SEG_COUNT : old_bottom;
        --b;
        bottom = b;
        atomic_thread_fence(memory_order_seq_cst);
        size_t t = atomic_load_explicit(&top, memory_order_acquire);

        if(t!=old_bottom){ //deque is empty
            bottom=t;
            return nullptr;
        } else{ //deque is not empty
            T* item = &segment(b)->buffer[offset(b)];

            if (t == b) { // 最后一个元素
                if (!atomic_compare_exchange_strong_explicit(
                        &top, &t, t,
                        memory_order_seq_cst, memory_order_seq_cst)) {
                    bottom = old_bottom;
                    return nullptr; //element is stolen by theft
                }
            }
            return item;
        }
    }

    const T* pop_front() {
        size_t t = atomic_load_explicit(&top, memory_order_acquire);
        size_t b = bottom;

        if (t != b) { // deque is not empty
            T* item = &segment(t)->buffer[offset(t)];
            size_t t_next=t+1;
            if(t_next==SEG_SIZE*SEG_COUNT) t_next=0;
            if (atomic_compare_exchange_strong_explicit(
                    &top, &t, t_next,
                    memory_order_seq_cst, memory_order_seq_cst)) {
                return item; // 偷取成功
            } else {
                return nullptr; // CAS 失败
            }
        } else { // deque is empty
            return nullptr;
        }
    }
};

}

#endif