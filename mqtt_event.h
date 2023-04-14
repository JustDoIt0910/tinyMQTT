//
// Created by zr on 23-4-5.
//

#ifndef TINYMQTT_MQTT_EVENT_H
#define TINYMQTT_MQTT_EVENT_H
#include "mqtt_map.h"
#include "mqtt_vec.h"
#include "mqtt_socket.h"
#include <sys/epoll.h>
#include <sys/queue.h>

typedef void(*tmq_event_cb)(tmq_socket_t, short, const void*);

typedef struct tmq_event_handler_s
{
    SLIST_ENTRY(tmq_event_s) event_next;
    tmq_socket_t fd;
    short events;
    short r_events;
    void* arg;
    tmq_event_cb cb;
} tmq_event_handler_t;

typedef SLIST_HEAD(tmq_event_handler_queue_s, tmq_event_handler_t) tmq_event_handler_queue_t;
typedef tmq_map(int, tmq_event_handler_queue_t) tmq_handler_map;
typedef tmq_vec(struct epoll_event) tmq_income_events;
typedef tmq_vec(tmq_event_handler_t*) tmq_active_handlers;

typedef struct tmq_event_loop_s
{
    int epoll_fd;
    tmq_income_events epoll_events;
    tmq_active_handlers active_handlers;
    tmq_handler_map handler_map;
} tmq_event_loop_t;

#endif //TINYMQTT_MQTT_EVENT_H
