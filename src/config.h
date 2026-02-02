#pragma once

// number of workers to execute the coroutines
#define SCHEDULER_NUM_WORKERS 10
#define TIMEWHEEL_INTERVAL_MS 1
#define TIMEWHEEL_NUM_SLOTS_PER_WHEEL 60
#define TIMEWHEEL_NUM_WHEELS 3
#define STACK_SIZE 1024 * 1024

// max number of threads that use Epoch Based Reclamation
#define EBR_MAX_THREADS 1024
#define EBR_ADVANCE_EPOCH_INTERVAL_MS 20

#define TS_LINKED_LIST_NODE_CACHE_MAX_THREADS 1024
#define TS_LINKED_LIST_NODE_CACHE_COUNT 128
#define TS_LINKED_LIST_NODE_CACHE_BATCH_RELEASE_COUNT 64

// mask for tagged pointer
#define TAGGED_PTR_MASK 0b111llu

//==========Utility Functions==========
#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))