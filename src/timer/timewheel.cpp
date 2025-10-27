#include "timewheel.h"
#include <time.h>
#include <errno.h>
#include "coroutine/coroutine.h"
#include "scheduler.h"

namespace rockcoro{

Timer Timer::inst;

TimeWheelLinkedListNode::TimeWheelLinkedListNode(Coroutine* coroutine)
    :next(nullptr), coroutine(coroutine){
}

TimeWheelLinkedList::TimeWheelLinkedList()
    :head(nullptr), tail(nullptr){
}
// pop an element from head. returns nullptr if empty
Coroutine* TimeWheelLinkedList::pop_front(){
    if(head==nullptr) return nullptr;
    TimeWheelLinkedListNode* ret=head;
    head=head->next;
    if(head==nullptr) tail=nullptr;
    return ret->coroutine;
}
void TimeWheelLinkedList::push_back(Coroutine* coroutine){
    if(head==nullptr){
        head=&coroutine->timewheel_node;
        tail=head;
    } else{
        tail->next=&coroutine->timewheel_node;
        tail=tail->next;
    }
    tail->next=nullptr;
}

TimeWheel::TimeWheel()
    :lower(nullptr), upper(nullptr){
}

void TimeWheel::add_event(Coroutine* coroutine, int delayMS){
    if(delayMS>=intervalMS*TIMEWHEEL_NUM_SLOTS_PER_WHEEL){ // add to upper timewheel
        if(upper)
            upper->add_event(coroutine, delayMS);
        else
            throw "cannot add event with such long interval";
    } else{ // add to this timewheel
        coroutine->timewheel_node->delayMS=delayMS;
        int slot_idx=(cur_slot_idx+delayMS/intervalMS)%(TIMEWHEEL_NUM_SLOTS_PER_WHEEL);
        slots[slot_idx].push_back(coroutine);
    }
}

void TimeWheel::tick(){
    TimeWheelLinkedList& cur_slot=slots[cur_slot_idx];
    if(lower)
        while(TimeWheelLinkedListNode* node=cur_slot.pop_front()){
            //add the node to the lower timewheel
            lower->add_event(node->coroutine, node->delayMS-intervalMS*TIMEWHEEL_NUM_SLOTS_PER_WHEEL);
        }
    else{
        while(TimeWheelLinkedListNode* node=cur_slot.pop_front()){
            Scheduler::inst.job_push(node->coroutine);
        }
    }
    cur_slot_idx++;
    if(cur_slot_idx>=TIMEWHEEL_NUM_SLOTS_PER_WHEEL){
        cur_slot_idx=0;
        if(upper) upper->tick();
    }
}

Timer::Timer(){
    pthread_spin_init(&spin_pending_events, 0);
    lowest_timewheel=timewheels[0];
    lowest_timewheel->lower=nullptr;
    lowest_timewheel->intervalMS=TIMEWHEEL_INTERVAL_MS;
    TimeWheel* cur_wheel=lowest_timewheel;
    for(int i=1;i<TIMEWHEEL_NUM_WHEELS;++i){
        cur_wheel->upper=&timewheels[i];
        cur_wheel->upper->lower=cur_wheel;
        cur_wheel->upper->intervalMS=cur_wheel->intervalMS*TIMEWHEEL_NUM_SLOTS_PER_WHEEL;
        cur_wheel=cur_wheel->upper;
    }
}

Timer::~Timer(){
    pthread_spin_destroy(&spin_pending_events);
}

void* Timer::event_loop(void*){
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (true) {
        // add pending events
        while(TimeWheelLinkedListNode* pending_event=Timer::inst.pop_pending_event()){
            lowest_timewheel->add_event(pending_event, pending_event->delayMS);
        }

        // tick timewheel
        lowest_timewheel->tick();

        // calculate next tick time
        next.tv_nsec += TIMEWHEEL_INTERVAL_MS * 1000000LL;
        while (next.tv_nsec >= 1000000000LL) {
            next.tv_nsec -= 1000000000LL;
            next.tv_sec++;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
}

void Timer::add_event(Coroutine* coroutine, int delayMS){
    coroutine->timewheel_node.delayMS=delayMS;
    push_pending_event(coroutine);
}

void Timer::push_pending_event(Coroutine* coroutine){
    pthread_spin_lock(&spin_pending_events);
    pending_events.push_back(coroutine);
    pthread_spin_unlock(&spin_pending_events);
}

Coroutine* Timer::pop_pending_event(){
    pthread_spin_lock(&spin_pending_events);
    Coroutine* ret=pending_events.pop_front();
    pthread_spin_unlock(&spin_pending_events);
    return ret;
}

}