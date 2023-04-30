//
// Created by zr on 23-4-18.
//

#ifndef TINYMQTT_MQTT_TIMER_H
#define TINYMQTT_MQTT_TIMER_H
#include <stdint.h>
#include <stddef.h>
#include "mqtt_vec.h"

#define TIMER_HEAP_INITIAL_SIZE     16
#define LEFT_CHILD_IDX(i)           ((i) << 1)
#define RIGHT_CHILD_IDX(i)          (((i) << 1) + 1)
#define PARENT_IDX(i)               ((i) >> 1)

typedef void(*tmq_timer_cb)(void* arg);

typedef struct tmq_timer_s
{
    int64_t expire;
    double timeout_ms;
    tmq_timer_cb cb;
    void* arg;
    int repeat;
    int canceled;
} tmq_timer_t;

typedef tmq_vec(tmq_timer_t*) timer_list;
typedef struct tmq_event_loop_s tmq_event_loop_t;

typedef struct tmq_timer_heap_s
{
    int timer_fd;
    tmq_timer_t** heap;
    size_t size;
    size_t cap;
    timer_list expired_timers;
} tmq_timer_heap_t;

void tmq_timer_heap_init(tmq_timer_heap_t* timer_heap, tmq_event_loop_t* loop);
void tmq_timer_heap_free(tmq_timer_heap_t* timer_heap);
void tmq_timer_heap_add(tmq_timer_heap_t* timer_heap, tmq_timer_t* timer);
/* for debug */
void tmq_timer_heap_print(tmq_timer_heap_t* timer_heap);
tmq_timer_t* tmq_timer_new(double timeout_ms, int repeat, tmq_timer_cb cb, void* arg);
void tmq_timer_reset(tmq_timer_t* timer);
void tmq_cancel_timer(tmq_timer_t* timer);

#endif //TINYMQTT_MQTT_TIMER_H
