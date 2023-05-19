//
// Created by zr on 23-4-5.
//

#ifndef TINYMQTT_MQTT_EVENT_H
#define TINYMQTT_MQTT_EVENT_H
#include "base/mqtt_map.h"
#include "base/mqtt_vec.h"
#include "base/mqtt_socket.h"
#include "mqtt_timer.h"
#include <sys/epoll.h>
#include <sys/queue.h>
#include <pthread.h>

#define INITIAL_EVENTLIST_SIZE 16
#define EPOLL_WAIT_TIMEOUT  10 * 1000

#define atomicSet(var, value)       __atomic_store_n(&(var), (value), __ATOMIC_SEQ_CST)
#define atomicGet(var)              __atomic_load_n (&(var), __ATOMIC_SEQ_CST)
#define atomicExchange(var, val)    __atomic_exchange_n(&(var), val, __ATOMIC_SEQ_CST)
#define decrementAndGet(var, val)   __atomic_sub_fetch(&(var), val, __ATOMIC_SEQ_CST)
#define incrementAndGet(var, val)   __atomic_add_fetch(&(var), val, __ATOMIC_SEQ_CST)

typedef void(*tmq_event_cb)(tmq_socket_t, uint32_t, const void*);

typedef struct tmq_event_handler_s
{
    SLIST_ENTRY(tmq_event_handler_s) event_next;
    tmq_socket_t fd;
    uint32_t events;
    uint32_t r_events;
    void* arg;
    tmq_event_cb cb;
} tmq_event_handler_t;

tmq_event_handler_t* tmq_event_handler_create(int fd, short events, tmq_event_cb cb, void* arg);

typedef SLIST_HEAD(handler_queue, tmq_event_handler_s) handler_queue;
typedef struct
{
    handler_queue handlers;
    uint32_t all_events;
} epoll_handler_ctx;
typedef tmq_map(int, epoll_handler_ctx) handler_map_t;
typedef tmq_vec(struct epoll_event) event_list_t;
typedef tmq_vec(tmq_event_handler_t*) active_handler_list_t;
typedef tmq_map(tmq_event_handler_t*, int) removing_handler_set_t;

typedef struct tmq_event_loop_s
{
    int epoll_fd;
    event_list_t epoll_events;

    active_handler_list_t active_handlers;
    removing_handler_set_t removing_handlers;
    handler_map_t handler_map;

    tmq_timer_heap_t timer_heap;
    int running;
    int quit;
    int event_handling;
    pthread_mutex_t lk;
} tmq_event_loop_t;

void tmq_event_loop_init(tmq_event_loop_t* loop);
void tmq_event_loop_run(tmq_event_loop_t* loop);
void tmq_handler_register(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
void tmq_handler_unregister(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
int tmq_handler_is_registered(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
void tmq_event_loop_add_timer(tmq_event_loop_t* loop, tmq_timer_t* timer);
void tmq_event_loop_cancel_timer(tmq_event_loop_t* loop, tmq_timer_t* timer);
void tmq_event_loop_quit(tmq_event_loop_t* loop);
void tmq_event_loop_destroy(tmq_event_loop_t* loop);

typedef void (*tmq_notify_cb) (void*);
typedef struct tmq_notifier_s
{
    tmq_event_loop_t* loop;
    int wakeup_pipe[2];
    tmq_event_handler_t* wakeup_handler;
    tmq_notify_cb cb;
    void* arg;
} tmq_notifier_t;

void tmq_notifier_init(tmq_notifier_t* notifier, tmq_event_loop_t* loop, tmq_notify_cb cb, void* arg);
void tmq_notifier_notify(tmq_notifier_t* notifier);
void tmq_notifier_destroy(tmq_notifier_t* notifier);

#endif //TINYMQTT_MQTT_EVENT_H
