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

#define INITIAL_EVENT_LIST_SIZE 64
#define EPOLL_WAIT_TIMEOUT  (10 * 1000)

typedef void(*tmq_event_cb)(tmq_socket_t, uint32_t, void*);

typedef struct tmq_event_handler_s
{
    SLIST_ENTRY(tmq_event_handler_s)
    next_handler;
    tmq_socket_t fd;
    uint32_t events;
    uint32_t r_events;
    void* arg;
    tmq_event_cb cb;
    int canceled;
    int tied;
} tmq_event_handler_t;

tmq_event_handler_t* tmq_event_handler_new(int fd, short events, tmq_event_cb cb, void* arg, int tie);


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


typedef SLIST_HEAD(handler_list_s, tmq_event_handler_s) handler_queue_t;
typedef struct
{
    handler_queue_t handlers;
    uint32_t all_events;
} epoll_channel_t;

typedef tmq_map(int, epoll_channel_t*) channel_map_t;
typedef tmq_vec(struct epoll_event) event_list_t;
typedef tmq_vec(tmq_event_handler_t*) handler_list_t;

typedef struct tmq_event_loop_s
{
    int epoll_fd;
    event_list_t epoll_events;
    handler_list_t active_handlers;
    handler_list_t removing_handlers;
    channel_map_t channels;
    tmq_timer_heap_t timer_heap;
    tmq_notifier_t quit_notifier;
    int running;
    int quit;
} tmq_event_loop_t;

#define REF_COUNTED_MEMBERS int ref_cnt;            \
                            clean_up_func cleaner;

typedef struct tmq_ref_counted_s tmq_ref_counted_t;
typedef void (*clean_up_func)(struct tmq_ref_counted_s*);
typedef struct tmq_ref_counted_s
{
    REF_COUNTED_MEMBERS
} tmq_ref_counted_t;

tmq_ref_counted_t* get_ref(tmq_ref_counted_t* obj);
void release_ref(tmq_ref_counted_t* handler);

void tmq_event_loop_init(tmq_event_loop_t* loop);
void tmq_event_loop_run(tmq_event_loop_t* loop);
void tmq_handler_register(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
void tmq_handler_unregister(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
int tmq_handler_is_registered(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
tmq_timer_id_t tmq_event_loop_add_timer(tmq_event_loop_t* loop, tmq_timer_t* timer);
void tmq_event_loop_cancel_timer(tmq_event_loop_t* loop, tmq_timer_id_t timer_id);
int tmq_event_loop_resume_timer(tmq_event_loop_t* loop, tmq_timer_id_t timer_id);
void tmq_event_loop_quit(tmq_event_loop_t* loop);
void tmq_event_loop_destroy(tmq_event_loop_t* loop);

#endif //TINYMQTT_MQTT_EVENT_H
