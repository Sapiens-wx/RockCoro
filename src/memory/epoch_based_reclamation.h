#pragma once
#include <atomic>
#include "basic_struct/data_structures.h"
#include "config.h"

namespace rockcoro {

struct TSLinkedListNode;

struct ThreadEpoch {
    std::atomic<int> epoch;
    // is this thread in an epoch
    std::atomic<bool> active;
    Vector<void *> retire_list[EBR_RING_BUFFER_LENGTH];
};

// epoch based reclamation
struct EpochBasedReclamation {
    static EpochBasedReclamation inst;
    std::atomic<int> global_epoch = 0;
    ThreadEpoch thread_epochs[EBR_MAX_THREADS];
    std::atomic<int> thread_epochs_count = 0;
    // number of threads active in each epoch
    std::atomic<int> thread_count_in_epoch[EBR_RING_BUFFER_LENGTH];
    std::atomic<bool> is_advancing_epoch = false;

    void init();
    void destroy();
    // only those threads that call this function are able to use EpochBasedReclamation
    // (they will be assigned with a ThreadEpoch instance from [thread_epochs])
    void init_thread_epoch();
    void enter_epoch();
    void exit_epoch();
    void retire(TSLinkedListNode *ptr);
    int get_thread_epoch_index();

private:
    void try_advance_epoch();
};

} // namespace rockcoro