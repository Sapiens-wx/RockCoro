#include "memory/epoch_based_reclamation.h"
#include <cstdlib>
#include <thread>
#include "memory/ts_linked_list.h"

namespace rockcoro {

#define GET_EPOCH_INDEX(epoch) ((epoch) % EBR_RING_BUFFER_LENGTH)

thread_local int thread_epoch_index = -1;

/*static void chaos_yield()
{
    if (rand() % 7 == 0)
        std::this_thread::yield();
}*/

int EpochBasedReclamation::get_thread_epoch_index()
{
    return thread_epoch_index;
}

void EpochBasedReclamation::init()
{
    for (int i = 0; i < EBR_RING_BUFFER_LENGTH; ++i) {
        thread_count_in_epoch[i].store(0);
    }
}
void EpochBasedReclamation::destroy()
{
    int thread_epochs_count_tmp = thread_epochs_count.load();
    for (int i = 0; i < thread_epochs_count_tmp; ++i) {
        ThreadEpoch &thread_epoch = thread_epochs[i];
        for (int i = 0; i < EBR_RING_BUFFER_LENGTH; ++i) {
            auto &retire_list = thread_epoch.retire_list[i];
            for (int j = retire_list.size() - 1; j >= 0; --j) {
                TSLinkedListNodeAllocator::inst.release((TSLinkedListNode *)retire_list[j]);
            }
            retire_list.clear();
        }
    }
}

void EpochBasedReclamation::init_thread_epoch()
{
    thread_epoch_index = thread_epochs_count.fetch_add(1);
    assert(thread_epoch_index < EBR_MAX_THREADS);
}

void EpochBasedReclamation::enter_epoch()
{
    ThreadEpoch &thread_epoch = thread_epochs[thread_epoch_index];
    thread_epoch.active.store(true);
    int prev_epoch = -1;
    int cur_epoch = global_epoch.load(std::memory_order_acquire);
    do {
        if (prev_epoch != -1)
            thread_count_in_epoch[GET_EPOCH_INDEX(prev_epoch)].fetch_sub(1);
        thread_epoch.epoch.store(cur_epoch);
        thread_count_in_epoch[GET_EPOCH_INDEX(cur_epoch)].fetch_add(1);
        prev_epoch = cur_epoch;
    } while (!global_epoch.compare_exchange_weak(cur_epoch, cur_epoch));
}
void EpochBasedReclamation::exit_epoch()
{
    ThreadEpoch &thread_epoch = thread_epochs[thread_epoch_index];
    thread_epoch.active.store(false);
    thread_count_in_epoch[GET_EPOCH_INDEX(thread_epoch.epoch.load())].fetch_sub(1);
    try_advance_epoch();
}
void EpochBasedReclamation::retire(TSLinkedListNode *ptr)
{
    assert(thread_epoch_index >= 0);
    ThreadEpoch &thread_epoch = thread_epochs[thread_epoch_index];
    thread_epoch.retire_list[GET_EPOCH_INDEX(thread_epoch.epoch.load())].push_back(ptr);
}
void EpochBasedReclamation::try_advance_epoch()
{
    bool b_is_advancing_epoch = is_advancing_epoch.load();
    if (!b_is_advancing_epoch &&
        is_advancing_epoch.compare_exchange_strong(b_is_advancing_epoch, true)) {
        int last_epoch = global_epoch.load() - 1 + EBR_RING_BUFFER_LENGTH;
        // check all the counts in ring buffer (except current epoch)
        // if there is an epoch whose thread_count>0, then we cannot advance epoch
        for (int i = last_epoch + 2; i < last_epoch + 1 + EBR_RING_BUFFER_LENGTH; ++i) {
            if (thread_count_in_epoch[GET_EPOCH_INDEX(i)].load() > 0) {
                is_advancing_epoch.store(false);
                return;
            }
        }

        int thread_epochs_count_tmp = thread_epochs_count.load();
        int epoch_index = GET_EPOCH_INDEX(last_epoch);
        int ptr_release_count = 0;
        for (int i = 0; i < thread_epochs_count_tmp; ++i) {
            ThreadEpoch &thread_epoch = thread_epochs[i];
            auto &retire_list = thread_epoch.retire_list[epoch_index];
            for (int j = 0; (size_t)j < retire_list.size(); ++j) {
                // free retire_list[i]
                assert(((TSLinkedListNode *)retire_list[j])->used_by_thread_epoch.load() == -1);
                TSLinkedListNodeAllocator::inst.release((TSLinkedListNode *)retire_list[j]);
                ++ptr_release_count;
            }
            retire_list.clear();
        }
        global_epoch.fetch_add(1);
        is_advancing_epoch.store(false);
    }
}

} // namespace rockcoro