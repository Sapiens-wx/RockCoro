#include <atomic>
#include <gtest/gtest.h>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <unordered_set>
#include <vector>
#include "log.h"
#include "memory/epoch_based_reclamation.h"
#include "scheduler.h"

using namespace rockcoro;

// tests both TSLinkedListNodeAllocator and EpochBasedReclamation
TEST(NodeAllocatorTest, MemoryLeakTest)
{
    const constexpr int NUM_BATCHES = 256, BATCH_SIZE = 1024;
    EpochBasedReclamation::inst.init_thread_epoch();
    for (int i = 0; i < NUM_BATCHES; ++i) {
        EpochBasedReclamation::inst.enter_epoch();
        for (int j = 0; j < BATCH_SIZE; ++j) {
            EpochBasedReclamation::inst.retire(TSLinkedListNodeAllocator::inst.get());
        }
        EpochBasedReclamation::inst.exit_epoch();
    }
}
