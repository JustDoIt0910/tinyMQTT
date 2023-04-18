//
// Created by zr on 23-4-18.
//
#include "mqtt_event.h"

int main()
{
    tmq_event_loop_t loop;
    tmq_event_loop_init(&loop);

    tmq_event_handler_t* handler = tmq_event_handler_create(1, 0, NULL, NULL);

    tmq_event_loop_register(&loop, handler);

    return 0;
}