#include "memory/epoch_based_reclamation.h"
#include <cstdlib>
#include "memory/ts_linked_list.h"

namespace rockcoro {

// epoch: current epoch
// active: true or false; whether the thread is in an epoch
#define GET_EPOCH_STATUS(epoch, active)                                                            \
    ((active) ? ((epoch) | 0x8000000000000000) : ((epoch) & 0x7FFFFFFFFFFFFFFF))

thread_local int thread_epoch_index = 0;

static void *epoch_maintainer(void *)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = EBR_ADVANCE_EPOCH_INTERVAL_MS * 1000 * 1000; // 20ms

    while (EpochBasedReclamation::inst.is_running.load()) {
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
        &epoch_maintainer_thread, nullptr, (void *(*)(void *)) & epoch_maintainer, nullptr);
}
void EpochBasedReclamation::destroy()
{
    int thread_epochs_count_tmp = thread_epochs_count.load();
    for (int i = 0; i < thread_epochs_count_tmp; ++i) {
        ThreadEpoch &thread_epoch = thread_epochs[i];
        auto &retire_list = thread_epoch.retire_list;
        while (retire_list.size()) {
            TSLinkedListNodeAllocator::inst.release(retire_list.front().ptr);
            retire_list.pop_front();
        }
    }
    is_running.store(false);
    pthread_join(epoch_maintainer_thread, nullptr);
}

void EpochBasedReclamation::init_thread_epoch()
{
    thread_epoch_index = thread_epochs_count.fetch_add(1);
    assert(thread_epoch_index < EBR_MAX_THREADS);
}

void EpochBasedReclamation::enter_epoch()
{
    ThreadEpoch &thread_epoch = thread_epochs[thread_epoch_index];
    uint64_t old_local_epoch = thread_epoch.epoch_status.load();
    uint64_t new_local_epoch;
    do {
        new_local_epoch = GET_EPOCH_STATUS(global_epoch.load(), true);
    } while (!thread_epoch.epoch_status.compare_exchange_weak(old_local_epoch, new_local_epoch));
}
void EpochBasedReclamation::exit_epoch()
{
    ThreadEpoch &thread_epoch = thread_epochs[thread_epoch_index];
    uint64_t old_local_epoch = thread_epoch.epoch_status.load();
    while (!thread_epoch.epoch_status.compare_exchange_weak(
        old_local_epoch, GET_EPOCH_STATUS(old_local_epoch, false)))
        ;
    // reclaim (GC)
    auto &retire_list = thread_epoch.retire_list;
    uint64_t cur_gc_epoch = gc_epoch.load();
    while (retire_list.size() && retire_list.front().retire_epoch <= cur_gc_epoch) {
        TSLinkedListNodeAllocator::inst.release(retire_list.front().ptr);
        retire_list.pop_front();
    }
}
void EpochBasedReclamation::retire(TSLinkedListNode *ptr)
{
    assert(thread_epoch_index >= 0);
    ThreadEpoch &thread_epoch = thread_epochs[thread_epoch_index];
    uint64_t cur_epoch = GET_EPOCH_STATUS(global_epoch.load(), false);
    thread_epoch.retire_list.push_back({ptr, cur_epoch});
}
void EpochBasedReclamation::advance_epoch()
{
    uint64_t min_epoch = global_epoch.load();
    global_epoch.fetch_add(1);
    uint64_t cur_glb_epoch = global_epoch.load();
    for (int i = thread_epochs_count.load() - 1; i >= 0; --i) {
        ThreadEpoch &thread_epoch = thread_epochs[i];
        uint64_t old_local_epoch = GET_EPOCH_STATUS(thread_epoch.epoch_status.load(), false);
        if (!thread_epoch.epoch_status.compare_exchange_strong(old_local_epoch, cur_glb_epoch)) {
            old_local_epoch = GET_EPOCH_STATUS(old_local_epoch, false);
            if (min_epoch > old_local_epoch)
                min_epoch = old_local_epoch;
        }
    }
    assert(min_epoch > 0);
    gc_epoch.store(min_epoch - 1);
}

} // namespace rockcoro