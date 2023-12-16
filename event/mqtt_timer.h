//
// Created by zr on 23-4-18.
//

#ifndef TINYMQTT_MQTT_TIMER_H
#define TINYMQTT_MQTT_TIMER_H
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include "base/mqtt_vec.h"
#include "base/mqtt_map.h"

#define SEC_US(sec)                 ((sec) * 1000000)
#define US_SEC(us)                  ((us) / 1000000)
#define SEC_MS(sec)                 ((sec) * 1000)
#define TIMER_HEAP_INITIAL_SIZE     16
#define LEFT_CHILD_IDX(i)           ((i) << 1)
#define RIGHT_CHILD_IDX(i)          (((i) << 1) + 1)
#define PARENT_IDX(i)               ((i) >> 1)

typedef struct tmq_timer_id_s
{
    int64_t addr;
    int64_t timestamp;
} tmq_timer_id_t;

typedef void(*tmq_timer_cb)(void* arg);

typedef struct tmq_timer_s
{
    int64_t expire;
    double timeout_ms;
    tmq_timer_cb cb;
    void* arg;
    int repeat;
    int canceled;
    tmq_timer_id_t timer_id;
} tmq_timer_t;

tmq_timer_id_t invalid_timer_id();

typedef tmq_vec(tmq_timer_t*) timer_list;
typedef tmq_map(tmq_timer_id_t, tmq_timer_t*) timer_id_map;
typedef struct tmq_event_loop_s tmq_event_loop_t;

typedef struct tmq_timer_heap_s
{
    int timer_fd;
    tmq_timer_t** heap;
    size_t size;
    size_t cap;
    timer_id_map registered_timers;
    timer_list expired_timers;
} tmq_timer_heap_t;

int64_t time_now();
void tmq_timer_heap_init(tmq_timer_heap_t* timer_heap, tmq_event_loop_t* loop);
void tmq_timer_heap_destroy(tmq_timer_heap_t* timer_heap);
tmq_timer_id_t tmq_timer_heap_add(tmq_timer_heap_t* timer_heap, tmq_timer_t* timer);
/* for debug */
void tmq_timer_heap_print(tmq_timer_heap_t* timer_heap);
tmq_timer_t* tmq_timer_new(double timeout_ms, int repeat, tmq_timer_cb cb, void* arg);
void tmq_cancel_timer(tmq_timer_heap_t* timer_heap, tmq_timer_id_t timer_id);
int tmq_resume_timer(tmq_timer_heap_t* timer_heap, tmq_timer_id_t timer_id);

#endif //TINYMQTT_MQTT_TIMER_H
