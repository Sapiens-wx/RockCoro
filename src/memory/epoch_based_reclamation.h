#pragma once
#include <atomic>
#include <pthread.h>
#include "basic_struct/data_structures.h"
#include "config.h"

namespace rockcoro {

struct TSLinkedListNode;

struct RetireRecord {
    TSLinkedListNode *ptr;
    uint64_t retire_epoch;
};

struct ThreadEpoch {
    // stores both active/inactive status and epoch index
    // | 1 bit: active/inactive | 63 bits: epoch index |
    std::atomic<uint64_t> epoch_status = {0};
    Deque<RetireRecord> retire_list;
};

// epoch based reclamation
struct EpochBasedReclamation {
    static EpochBasedReclamation inst;
    std::atomic<uint64_t> global_epoch = 1;
    std::atomic<uint64_t> gc_epoch = 0;
    ThreadEpoch thread_epochs[EBR_MAX_THREADS];
    std::atomic<int> thread_epochs_count = 0;
    std::atomic<bool> is_running = true;
    pthread_t epoch_maintainer_thread;

    void init();
    void destroy();
    // only those threads that call this function are able to use EpochBasedReclamation
    // (they will be assigned with a ThreadEpoch instance from [thread_epochs])
    void init_thread_epoch();
    void enter_epoch();
    void exit_epoch();
    void retire(TSLinkedListNode *ptr);
    int get_thread_epoch_index();
    void advance_epoch();
};

} // namespace rockcoro