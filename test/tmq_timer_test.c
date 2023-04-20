//
// Created by zr on 23-4-20.
//
#include "mqtt_timer.h"
#include "mqtt_event.h"

double timeout[] = {2.0, 2.8, 1.2, 8.7, 3.4, 7.2, 21.9, 0.04, 0.8, 0.2, 0, 0.09, 87.3};

int main()
{
    tmq_event_loop_t loop;
    tmq_event_loop_init(&loop);

    tmq_timer_heap_t heap;
    tmq_timer_heap_init(&heap, &loop);

    for(int i = 0; i < 13; i++)
    {
        tmq_timer_t* timer = tmq_timer_new(timeout[i], 0, NULL, NULL);
        tmq_timer_heap_insert(&heap, timer);
    }

    tmq_timer_heap_print(&heap);

    return 0;
}