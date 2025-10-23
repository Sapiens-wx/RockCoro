#pragma once
#include <atomic>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include "log.h"

namespace rockcoro{

struct Coroutine;

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

template <typename T, size_t SEG_SIZE=1024, size_t SEG_COUNT=1024>
struct Queue {
    struct Segment {
        T buffer[SEG_SIZE];
    };

    Segment* data[SEG_COUNT];
    size_t top, bottom; // bottom is exclusive

    Queue() : top(0), bottom(0) {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            data[i] = nullptr;
    }

    ~Queue() {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            if (data[i]) delete data[i];
    }

    Segment* segment(size_t idx) { return data[idx / SEG_SIZE]; }
    size_t offset(size_t idx) { return idx % SEG_SIZE; }

    void push(const T& item) {
		size_t b_next=bottom+1;

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
    }

    /// @brief pop an item from the front.
    /// @return nullptr if the queue is empty
    T* pop() {
        T* item = nullptr;

        if (top != bottom) { // deque is not empty
            item = &segment(top)->buffer[offset(top)];
            top=top+1;
            if(top==SEG_SIZE*SEG_COUNT) top=0;
        }
        return item;
    }   
};

struct LinkedListNode{
    LinkedListNode* next;
    Coroutine* coroutine;

    LinkedListNode();
};

struct LinkedList{
    LinkedListNode* head;
    LinkedListNode* tail;

    LinkedList();
    // pop an element from head. returns nullptr if empty
    Coroutine* pop_front();
    void push_back(Coroutine* coroutine);
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
    std::atomic<size_t> top;
    size_t bottom; //exclusive

    CLDeque() : top(0), bottom(0) {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            data[i] = nullptr;
    }

    ~CLDeque() {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            if (data[i]) delete data[i];
    }

    Segment* segment(size_t idx) { return data[idx / SEG_SIZE]; }
    size_t offset(size_t idx) { return idx % SEG_SIZE; }

    void push_back(const T& item) {
        size_t b = bottom;
		size_t b_next=b+1;

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
    const T* pop_back() {
        size_t old_bottom=bottom;
        size_t b = old_bottom==0?SEG_SIZE*SEG_COUNT : old_bottom;
        --b;
        size_t t = top.load();
		size_t t_next=t+1;
		if(t_next>=SEG_SIZE*SEG_COUNT)
			t_next=0;

        if(top.load()==old_bottom){ //deque is empty
            return nullptr;
        } else{ //deque is not empty
			bottom = b;
			atomic_thread_fence(std::memory_order_seq_cst);
            T* item = &segment(b)->buffer[offset(b)];

            if (top.load() == b) { // is the last element
				if (top.compare_exchange_strong(
						t, t_next, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
					bottom=old_bottom; //bottom has to be set after top is CASed.
					return item; // CAS success
				} else{
					bottom=old_bottom;
					return nullptr; // stolen by a theft
				}
            }
            return item;
        }
    }

    const T* pop_front() {
        size_t t = top.load();
        size_t b = bottom;

        if (t != b) { // deque is not empty
            T* item = &segment(t)->buffer[offset(t)];
            size_t t_next=t+1;
            if(t_next==SEG_SIZE*SEG_COUNT) t_next=0;
            if (top.compare_exchange_strong(
                    t, t_next, std::memory_order_seq_cst, std::memory_order_seq_cst))
                return item; // CAS success
            else
                return nullptr; // CAS fail
        } else // deque is empty
            return nullptr;
    }
};

/// @brief Michael-Scott queue
template <typename T, size_t SEG_SIZE=1024, size_t SEG_COUNT=1024>
struct MSQueue {
    struct Segment {
        T buffer[SEG_SIZE];
    };

    Segment* data[SEG_COUNT];
    size_t top, bottom; // bottom is exclusive
    pthread_mutex_t mutex;

    MSQueue() : top(0), bottom(0) {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            data[i] = nullptr;
        pthread_mutex_init(&mutex, nullptr);
    }

    ~MSQueue() {
        for (size_t i = 0; i < SEG_COUNT; ++i)
            if (data[i]) delete data[i];
        pthread_mutex_destroy(&mutex);
    }

    Segment* segment(size_t idx) { return data[idx / SEG_SIZE]; }
    size_t offset(size_t idx) { return idx % SEG_SIZE; }

    void push(const T& item) {
        pthread_mutex_lock(&mutex);
		size_t b_next=bottom+1;

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
    const T* pop() {
        pthread_mutex_lock(&mutex);

        T* item = nullptr;

        if (top != bottom) { // deque is not empty
            item = &segment(top)->buffer[offset(top)];
            top=top+1;
            if(top==SEG_SIZE*SEG_COUNT) top=0;
        }
        pthread_mutex_unlock(&mutex);
        return item;
    }   
};

}
