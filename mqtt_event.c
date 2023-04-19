//
// Created by zr on 23-4-5.
//
#include "mqtt_event.h"
#include "tlog.h"
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

tmq_event_handler_t* tmq_event_handler_create(int fd, short events, tmq_event_cb cb, void* arg)
{
    tmq_event_handler_t* handler = (tmq_event_handler_t*) malloc(sizeof(tmq_event_handler_t));
    if(!handler)
        return NULL;
    bzero(handler, sizeof(tmq_event_handler_t));
    handler->fd = fd;
    handler->events = events;
    handler->arg = arg;
    handler->cb = cb;
    return handler;
}

void tmq_event_loop_init(tmq_event_loop_t* loop)
{
    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    tmq_vec_init(&loop->epoll_events, struct epoll_event);
    tmq_vec_init(&loop->active_handlers, tmq_event_handler_t*);
    tmq_map_32_init(&loop->handler_map, tmq_event_handler_queue_t,
                    MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_vec_resize(loop->epoll_events, INITIAL_EVENTLIST_SIZE);
    loop->running = 0;
    loop->quit = 0;
    pthread_mutex_init(&loop->lk, NULL);
}

void tmq_event_loop_run(tmq_event_loop_t* loop)
{
    if(!loop) return;
    if(atomicExchange(loop->running, 1) == 1)
        return;
    pthread_mutex_lock(&loop->lk);
    while(!atomicGet(loop->quit))
    {
        pthread_mutex_unlock(&loop->lk);
        int events_num = epoll_wait(loop->epoll_fd,
                                    tmq_vec_begin(loop->epoll_events),
                                    tmq_vec_size(loop->epoll_events),
                                    EPOLL_WAIT_TIMEOUT);
        pthread_mutex_lock(&loop->lk);
        if(events_num > 0)
        {
            if(events_num == tmq_vec_size(loop->epoll_events))
                tmq_vec_resize(loop->epoll_events, 2 * tmq_vec_size(loop->epoll_events));
            tmq_vec_clear(loop->active_handlers);
            for(int i = 0; i < events_num; i++)
            {
                struct epoll_event* event = tmq_vec_at(loop->epoll_events, i);
                assert(event != NULL);
                tmq_event_handler_queue_t* handlers = tmq_map_get(loop->handler_map, event->data.fd);
                if(!handlers)
                    continue;
                tmq_event_handler_t* handler;
                SLIST_FOREACH(handler, handlers, event_next)
                {
                    if(!(handler->events & event->events))
                        continue;
                    handler->r_events = event->events;
                    tmq_vec_push_back(loop->active_handlers, handler);
                }
            }
            tmq_event_handler_t** it;
            for(it = tmq_vec_begin(loop->active_handlers);
                it != tmq_vec_end(loop->active_handlers);
                it++)
            {
                tmq_event_handler_t* active_handler = *it;
                active_handler->cb(active_handler->fd, active_handler->r_events, active_handler->arg);
            }
        }
        else if(events_num < 0)
            tlog_error("epoll_wait() error %d: %s", errno, strerror(errno));
    }
    atomicSet(loop->running, 0);
    pthread_mutex_unlock(&loop->lk);
}

void tmq_event_loop_register(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    struct epoll_event event;
    bzero(&event, sizeof(struct epoll_event));
    event.data.fd = handler->fd;
    event.events = handler->events;

    pthread_mutex_lock(&loop->lk);
    if(epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, handler->fd, &event) < 0)
    {
        tlog_fatal("epoll_ctl() error %d: %s", errno, strerror(errno));
        tlog_exit();
        abort();
    }
    tmq_event_handler_queue_t* handler_queue = tmq_map_get(loop->handler_map, handler->fd);
    if(!handler_queue)
    {
        tmq_event_handler_queue_t hq;
        bzero(&hq, sizeof(tmq_event_handler_queue_t));
        tmq_map_put(loop->handler_map, handler->fd, hq);
    }
    handler_queue = tmq_map_get(loop->handler_map, handler->fd);
    assert(handler_queue != NULL);
    SLIST_INSERT_HEAD(handler_queue, handler, event_next);
    pthread_mutex_unlock(&loop->lk);
}

void tmq_event_loop_quit(tmq_event_loop_t* loop)
{
    atomicSet(loop->quit, 1);
}

void tmq_event_loop_clean(tmq_event_loop_t* loop)
{
    if(atomicGet(loop->running))
        return;
    tmq_vec_free(loop->epoll_events);
    tmq_vec_free(loop->active_handlers);
    tmq_map_iter_t it;
    for(it = tmq_map_iter(loop->handler_map); tmq_map_has_next(it); tmq_map_next(loop->handler_map, it))
    {
        tmq_event_handler_queue_t* handler_queue = it.entry->value;
        tmq_event_handler_t* handler, *next;
        handler = handler_queue->slh_first;
        while(handler)
        {
            next = handler->event_next.sle_next;
            free(handler);
            handler = next;
        }
    }
    tmq_map_free(loop->handler_map);
    close(loop->epoll_fd);
}