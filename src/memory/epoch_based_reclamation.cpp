#include "memory/epoch_based_reclamation.h"
#include <cstdlib>
#include "memory/ts_linked_list.h"

namespace rockcoro {

// epoch: current epoch
// active: true or false; whether the thread is in an epoch
#define GET_EPOCH_STATUS(epoch, active)                                                            \
    ((active) ? ((epoch) | 0x8000000000000000) : ((epoch) & 0x7FFFFFFFFFFFFFFF))

// given an epoch status, returns whether the epoch status is active or not
#define IS_EPOCH_STATUS_ACTIVE(epoch_status) !!((epoch_status) & 0x8000000000000000)

thread_local int thread_epoch_index = -1;

void *EpochBasedReclamation::epoch_maintainer(void *)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = EBR_ADVANCE_EPOCH_INTERVAL_MS * 1000 * 1000; // 20ms

    while (EpochBasedReclamation::inst.is_running_.load(std::memory_order_relaxed)) {
        EpochBasedReclamation::inst.advance_epoch();
        nanosleep(&ts, NULL);
    }
    return nullptr;
}

int EpochBasedReclamation::get_thread_epoch_index()
{
    return thread_epoch_index;
}

void EpochBasedReclamation::init()
{
    pthread_create(
        &epoch_maintainer_thread_, nullptr, (void *(*)(void *)) & epoch_maintainer, nullptr);
}
void EpochBasedReclamation::destroy()
{
    int thread_epochs_count_tmp = thread_epochs_count_.load(std::memory_order_acquire);
    for (int i = 0; i < thread_epochs_count_tmp; ++i) {
        ThreadEpoch &thread_epoch = thread_epochs_[i];
        auto &retire_list = thread_epoch.retire_list_;
        while (retire_list.size()) {
            TSLinkedListNodeAllocator::inst.release(retire_list.front().ptr_);
            retire_list.pop_front();
        }
    }
    is_running_.store(false, std::memory_order_relaxed);
    pthread_join(epoch_maintainer_thread_, nullptr);
}

void EpochBasedReclamation::init_thread_epoch()
{
    if (thread_epoch_index == -1) { // avoid duplicate calls
        thread_epoch_index = thread_epochs_count_.fetch_add(1, std::memory_order_release);
        assert(thread_epoch_index < EBR_MAX_THREADS);
    }
}

void EpochBasedReclamation::enter_epoch()
{
    init_thread_epoch();
    ThreadEpoch &thread_epoch = thread_epochs_[thread_epoch_index];
    uint64_t old_local_epoch = thread_epoch.epoch_status_.load(std::memory_order_acquire);
#ifndef NDEBUG
    assert(!IS_EPOCH_STATUS_ACTIVE(old_local_epoch)); // duplicate call to enter_epoch()
#endif
    if (IS_EPOCH_STATUS_ACTIVE(old_local_epoch)) {
        return; // duplicate call to enter_epoch()
    }
    uint64_t new_local_epoch;
    do {
        new_local_epoch = GET_EPOCH_STATUS(global_epoch_.load(std::memory_order_acquire), true);
    } while (!thread_epoch.epoch_status_.compare_exchange_weak(
        old_local_epoch, new_local_epoch, std::memory_order_acquire, std::memory_order_relaxed));
}
void EpochBasedReclamation::exit_epoch()
{
    init_thread_epoch();
    ThreadEpoch &thread_epoch = thread_epochs_[thread_epoch_index];
    uint64_t old_local_epoch = thread_epoch.epoch_status_.load(std::memory_order_acquire);
#ifndef NDEBUG
    assert(IS_EPOCH_STATUS_ACTIVE(old_local_epoch)); // duplicate call to exit_epoch()
#endif
    if (!IS_EPOCH_STATUS_ACTIVE(old_local_epoch))
        return; // duplicate call to exit_epoch()
    while (
        !thread_epoch.epoch_status_.compare_exchange_weak(old_local_epoch,
                                                          GET_EPOCH_STATUS(old_local_epoch, false),
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed))
        ;
    // reclaim (GC)
    auto &retire_list = thread_epoch.retire_list_;
    uint64_t cur_gc_epoch = gc_epoch_.load(std::memory_order_acquire);
    while (retire_list.size() && retire_list.front().retire_epoch_ <= cur_gc_epoch) {
        TSLinkedListNodeAllocator::inst.release(retire_list.front().ptr_);
        retire_list.pop_front();
    }
}
void EpochBasedReclamation::retire(TSLinkedListNodePtr ptr)
{
    assert(thread_epoch_index >= 0);
    ThreadEpoch &thread_epoch = thread_epochs_[thread_epoch_index];
    assert(IS_EPOCH_STATUS_ACTIVE(thread_epoch.epoch_status_.load(
        std::
            memory_order_acquire))); // retire must be called between enter_epoch() and exit_epoch()
    uint64_t cur_epoch = GET_EPOCH_STATUS(global_epoch_.load(std::memory_order_acquire), false);
    thread_epoch.retire_list_.push_back({ptr, cur_epoch});
}
void EpochBasedReclamation::advance_epoch()
{
    uint64_t min_epoch = global_epoch_.load(std::memory_order_acquire);
    global_epoch_.fetch_add(1, std::memory_order_acq_rel);
    uint64_t cur_glb_epoch = global_epoch_.load();
    for (int i = thread_epochs_count_.load(std::memory_order_acquire) - 1; i >= 0; --i) {
        ThreadEpoch &thread_epoch = thread_epochs_[i];
        uint64_t old_local_epoch =
            GET_EPOCH_STATUS(thread_epoch.epoch_status_.load(std::memory_order_acquire), false);
        if (!thread_epoch.epoch_status_.compare_exchange_strong(old_local_epoch,
                                                                cur_glb_epoch,
                                                                std::memory_order_acquire,
                                                                std::memory_order_relaxed)) {
            old_local_epoch = GET_EPOCH_STATUS(old_local_epoch, false);
            if (min_epoch > old_local_epoch)
                min_epoch = old_local_epoch;
        }
    }
    assert(min_epoch > 0);
    gc_epoch_.store(min_epoch - 1, std::memory_order_release);
}

} // namespace rockcoro