#pragma once
#include <pthread.h>

namespace rockcoro {

#define TIMEWHEEL_INTERVAL_MS 1
#define TIMEWHEEL_NUM_SLOTS_PER_WHEEL 60
#define TIMEWHEEL_NUM_WHEELS 3

struct Coroutine;

struct TimeWheelLinkedListNode {
    TimeWheelLinkedListNode *next = nullptr;
    Coroutine *coroutine = nullptr;
    int delayMS;

    TimeWheelLinkedListNode(Coroutine *coroutine);
};

struct TimeWheelLinkedList {
    TimeWheelLinkedListNode *head = nullptr;
    TimeWheelLinkedListNode *tail = nullptr;

    TimeWheelLinkedList();
    // pop an element from head. returns nullptr if empty
    TimeWheelLinkedListNode *pop_front();
    void push_back(Coroutine *coroutine);
};

struct TimeEvent {};

struct TimeWheel {
    TimeWheelLinkedList slots[TIMEWHEEL_NUM_SLOTS_PER_WHEEL];
    // pointer to the time wheel with larger interval
    TimeWheel *lower = nullptr, *upper = nullptr;
    int cur_slot_idx = 0;
    // the interval in ms between two slots
    int intervalMS;

    void tick();
    void add_event(Coroutine *coroutine);
};

struct TimerManager {
    static TimerManager inst;
    TimeWheel timewheels[TIMEWHEEL_NUM_WHEELS];
    TimeWheel *lowest_timewheel = nullptr;
    // coroutines that are added to the timewheel by TimerManager::add_event. (they are actually temporarily
    // added to this pending queue)
    // event_loop will actually add these events to the timewheel.
    TimeWheelLinkedList pending_events;
    pthread_spinlock_t spin_pending_events;

    TimerManager();
    ~TimerManager();
    // tick every [TIMEWHEEL_INTERVAL_MS] ms
    static void *event_loop(void *);
    void add_event(Coroutine *coroutine, int delayMS);

private:
    void push_pending_event(Coroutine *coroutine);
    TimeWheelLinkedListNode *pop_pending_event();
    // actually adds the coroutine to the timewheel. expire time will be stored in coroutine
    void internal_add_event(Coroutine *coroutine);
};

} // namespace rockcoro
