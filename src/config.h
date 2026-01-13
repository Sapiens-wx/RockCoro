#pragma once

// number of workers to execute the coroutines
#define SCHEDULER_NUM_WORKERS 10
#define TIMEWHEEL_INTERVAL_MS 1
#define TIMEWHEEL_NUM_SLOTS_PER_WHEEL 60
#define TIMEWHEEL_NUM_WHEELS 3
#define STACK_SIZE 1024 * 1024

// max number of threads that use Epoch Based Reclamation
#define EBR_MAX_THREADS 1110
#define EBR_RING_BUFFER_LENGTH 3

#define TS_LINKED_LIST_NODE_CACHE_COUNT 100

//==========Utility Functions==========
#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))