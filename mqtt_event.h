//
// Created by zr on 23-4-5.
//

#ifndef TINYMQTT_MQTT_EVENT_H
#define TINYMQTT_MQTT_EVENT_H
#include "mqtt_map.h"
#include "mqtt_vec.h"
#include "mqtt_socket.h"
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

#define getRef(obj)                 (incrementAndGet((obj)->ref_cnt, 1), (obj))
#define releaseRef(obj)             do                                                  \
                                    {                                                   \
                                        int __n = decrementAndGet((obj)->ref_cnt, 1);   \
                                        if(!__n)                                        \
                                        {                                               \
                                            obj->destroy_cb(obj);                       \
                                            free(obj);                                  \
                                        }                                               \
                                    } while(0)

typedef void(*tmq_destroy_cb)(void* obj);
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

typedef SLIST_HEAD(tmq_event_handler_queue_s, tmq_event_handler_s) tmq_event_handler_queue_t;
typedef tmq_map(int, tmq_event_handler_queue_t) tmq_handler_map;
typedef tmq_vec(struct epoll_event) tmq_income_events;
typedef tmq_vec(tmq_event_handler_t*) tmq_active_handlers;

typedef struct tmq_event_loop_s
{
    int epoll_fd;
    tmq_income_events epoll_events;
    tmq_active_handlers active_handlers;
    tmq_handler_map handler_map;
    tmq_timer_heap_t timer_heap;
    int running;
    int quit;
    pthread_mutex_t lk;
} tmq_event_loop_t;

void tmq_event_loop_init(tmq_event_loop_t* loop);
void tmq_event_loop_run(tmq_event_loop_t* loop);
void tmq_event_loop_register(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
void tmq_event_loop_unregister(tmq_event_loop_t* loop, tmq_event_handler_t* handler);
void tmq_event_loop_add_timer(tmq_event_loop_t* loop, tmq_timer_t* timer);
void tmq_event_loop_cancel_timer(tmq_event_loop_t* loop, tmq_timer_t* timer);
void tmq_event_loop_quit(tmq_event_loop_t* loop);
void tmq_event_loop_clean(tmq_event_loop_t* loop);

#endif //TINYMQTT_MQTT_EVENT_H
