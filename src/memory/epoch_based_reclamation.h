#pragma once
#include <atomic>
#include <pthread.h>
#include "basic_struct/deque.h"
#include "config.h"
#include "memory/ts_linked_list_node.h"

namespace rockcoro {

// max number of threads that use Epoch Based Reclamation
constexpr int EBR_MAX_THREADS = 1024;
constexpr size_t EBR_ADVANCE_EPOCH_INTERVAL_MS = 20;

struct RetireRecord {
    TSLinkedListNodePtr ptr_;
    uint64_t retire_epoch_;
};

struct ThreadEpoch {
    // stores both active/inactive status and epoch index
    // | 1 bit: active/inactive | 63 bits: epoch index |
    std::atomic<uint64_t> epoch_status_ = {0};
    Deque<RetireRecord, 8192> retire_list_;
};

// epoch based reclamation
struct EpochBasedReclamation {
public:
    static EpochBasedReclamation inst;

private:
    std::atomic<uint64_t> global_epoch_ = 1;
    std::atomic<uint64_t> gc_epoch_ = 0;
    ThreadEpoch thread_epochs_[EBR_MAX_THREADS];
    std::atomic<int> thread_epochs_count_ = 0;
    std::atomic<bool> is_running_ = true;
    pthread_t epoch_maintainer_thread_;

public:
    void init();
    void destroy();
    void enter_epoch();
    void exit_epoch();
    void retire(TSLinkedListNodePtr ptr);
    int get_thread_epoch_index();

private:
    // only those threads that call this function are able to use EpochBasedReclamation
    // (they will be assigned with a ThreadEpoch instance from [thread_epochs])
    void init_thread_epoch();
    void advance_epoch();
    static void *epoch_maintainer(void *);
};

} // namespace rockcoro