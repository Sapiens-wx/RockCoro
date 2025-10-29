#pragma once
#include <pthread.h>

namespace rockcoro
{

#define TIMEWHEEL_INTERVAL_MS 1
#define TIMEWHEEL_NUM_SLOTS_PER_WHEEL 60
#define TIMEWHEEL_NUM_WHEELS 3

    struct Coroutine;

    struct TimeWheelLinkedListNode
    {
        TimeWheelLinkedListNode *next;
        Coroutine *coroutine;
        int delayMS;

        TimeWheelLinkedListNode(Coroutine *coroutine);
    };

    struct TimeWheelLinkedList
    {
        TimeWheelLinkedListNode *head;
        TimeWheelLinkedListNode *tail;

        TimeWheelLinkedList();
        // pop an element from head. returns nullptr if empty
        Coroutine *pop_front();
        void push_back(Coroutine *coroutine);
    };

    struct TimeEvent
    {
    };

    struct TimeWheel
    {
        TimeWheelLinkedList slots[TIMEWHEEL_NUM_SLOTS_PER_WHEEL];
        // pointer to the time wheel with larger interval
        TimeWheel *lower, *upper;
        int cur_slot_idx;
        // the interval in ms between two slots
        int intervalMS;

        TimeWheel();
        void tick();
        void add_event(Coroutine *coroutine, int delayMS);
    };

    struct Timer
    {
        static Timer inst;
        TimeWheel timewheels[TIMEWHEEL_NUM_WHEELS];
        TimeWheel *lowest_timewheel;
        // coroutines that are added to the timewheel by Timer::add_event. (they are actually temporarily
        // added to this pending queue)
        // event_loop will actually add these events to the timewheel.
        TimeWheelLinkedList pending_events;
        pthread_spinlock_t spin_pending_events;

        Timer();
        ~Timer();
        // tick every [TIMEWHEEL_INTERVAL_MS] ms
        static void *event_loop(void *);
        void add_event(Coroutine *coroutine, int delayMS);

    private:
        void push_pending_event(Coroutine *coroutine);
        Coroutine *pop_pending_event();
    };

}