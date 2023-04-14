//
// Created by zr on 23-4-5.
//

#ifndef TINYMQTT_MQTT_EVENT_H
#define TINYMQTT_MQTT_EVENT_H
#include "mqtt_map.h"
#include <sys/queue.h>

typedef struct tmq_event_s
{
    SLIST_ENTRY(tmq_event_s) event_next;
} tmq_event_t;

typedef SLIST_HEAD(tmq_event_queue_s, tmq_event_t) tmq_event_queue_t;
typedef tmq_map(int, tmq_event_queue_t) tmq_io_map;

typedef struct tmq_event_loop_s
{
    int epoll_fd;
    tmq_io_map io_map;
} tmq_event_loop_t;

#endif //TINYMQTT_MQTT_EVENT_H
