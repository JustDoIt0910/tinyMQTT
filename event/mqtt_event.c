//
// Created by zr on 23-4-5.
//
#include "mqtt_event.h"
#include "tlog.h"
#include "base/mqtt_util.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

tmq_event_handler_t* tmq_event_handler_new(int fd, short events, tmq_event_cb cb, void* arg, int tie)
{
    tmq_event_handler_t* handler = malloc(sizeof(tmq_event_handler_t));
    if(!handler)
        fatal_error("malloc() error: out of memory");
    bzero(handler, sizeof(tmq_event_handler_t));
    handler->fd = fd;
    handler->events = events;
    handler->arg = arg;
    handler->cb = cb;
    handler->canceled = 0;
    handler->tied = tie;
    return handler;
}

tmq_ref_counted_t* get_ref(tmq_ref_counted_t* obj)
{
    incrementAndGet(obj->ref_cnt, 1);
    return obj;
}
void release_ref(tmq_ref_counted_t* obj)
{
    int n = decrementAndGet(obj->ref_cnt, 1);
    if(!n)
    {
        obj->cleaner(obj);
        free(obj);
    }
}

void tmq_event_loop_init(tmq_event_loop_t* loop)
{
    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if(loop->epoll_fd <= 0)
        fatal_error("epoll_create1() error: %s", strerror(errno));

    tmq_vec_init(&loop->epoll_events, struct epoll_event);
    tmq_vec_init(&loop->active_handlers, tmq_event_handler_t*);
    tmq_vec_init(&loop->removing_handlers, tmq_event_handler_t*);
    tmq_map_32_init(&loop->channels, epoll_channel_t , MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_vec_resize(loop->epoll_events, INITIAL_EVENT_LIST_SIZE);
    loop->running = 0;
    loop->quit = 0;
    tmq_timer_heap_init(&loop->timer_heap, loop);
    tmq_notifier_init(&loop->quit_notifier, loop, NULL, NULL);
}

void tmq_event_loop_run(tmq_event_loop_t* loop)
{
    if(!loop) return;
    if(atomicExchange(loop->running, 1) == 1)
        return;
    loop->quit = 0;
    while(!loop->quit)
    {
        int events_num = epoll_wait(loop->epoll_fd,
                                    tmq_vec_begin(loop->epoll_events),
                                    tmq_vec_size(loop->epoll_events),
                                    EPOLL_WAIT_TIMEOUT);
        if(events_num > 0)
        {
            if(events_num == tmq_vec_size(loop->epoll_events))
                tmq_vec_resize(loop->epoll_events, 2 * tmq_vec_size(loop->epoll_events));
            tmq_vec_clear(loop->active_handlers);
            for(int i = 0; i < events_num; i++)
            {
                struct epoll_event* event = tmq_vec_at(loop->epoll_events, i);
                assert(event != NULL);
                epoll_channel_t* channel = event->data.ptr;
                if(!channel) continue;
                tmq_event_handler_t* handler;
                SLIST_FOREACH(handler, &channel->handlers, next_handler)
                {
                    if(!(handler->events & event->events))
                        continue;
                    handler->r_events = event->events;
                    tmq_vec_push_back(loop->active_handlers, handler);
                }
            }
            tmq_vec_clear(loop->removing_handlers);
            tmq_event_handler_t** it = tmq_vec_begin(loop->active_handlers);
            for(; it != tmq_vec_end(loop->active_handlers); it++)
            {
                tmq_event_handler_t* active_handler = *it;
                if(!active_handler->canceled && active_handler->cb)
                    active_handler->cb(active_handler->fd, active_handler->r_events, active_handler->arg);
            }
            it = tmq_vec_begin(loop->removing_handlers);
            for(; it != tmq_vec_end(loop->removing_handlers); it++) {
                tmq_event_handler_t* removing = *it;
                if(removing->tied) release_ref(removing->arg);
            }
        }
        else if(events_num < 0)
            tlog_error("epoll_wait() error %d: %s", errno, strerror(errno));
    }
    atomicSet(loop->running, 0);
}

void tmq_handler_register(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    if(!loop || !handler) return;
    if(handler->tied) get_ref(handler->arg);
    struct epoll_event event;
    bzero(&event, sizeof(struct epoll_event));
    event.events = handler->events;

    int op = EPOLL_CTL_ADD;
    epoll_channel_t** channel = tmq_map_get(loop->channels, handler->fd);
    if(!channel)
    {
        epoll_channel_t* new_channel = malloc(sizeof(epoll_channel_t));
        bzero(new_channel, sizeof(epoll_channel_t));
        tmq_map_put(loop->channels, handler->fd, new_channel);
        event.data.ptr = new_channel;
        channel = &new_channel;
    }
    else
    {
        op = EPOLL_CTL_MOD;
        event.events = (*channel)->all_events | handler->events;
        event.data.ptr = *channel;
    }

    SLIST_INSERT_HEAD(&(*channel)->handlers, handler, next_handler);
    handler->canceled = 0;
    (*channel)->all_events = event.events;

    if(epoll_ctl(loop->epoll_fd, op, handler->fd, &event) < 0)
        fatal_error("epoll_ctl() error %d: %s", errno, strerror(errno));
}

void tmq_handler_unregister(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    if(!loop || !handler) return;
    epoll_channel_t** channel = tmq_map_get(loop->channels, handler->fd);
    if(!channel)
    {
        tlog_warn("handler already unregistered");
        return;
    }
    tmq_event_handler_t** next = &(*channel)->handlers.slh_first;
    while(*next && *next != handler)
        next = &(*next)->next_handler.sle_next;
    if(*next)
    {
        *next = (*next)->next_handler.sle_next;
        handler->next_handler.sle_next = NULL;
        handler->canceled = 1;
        int op = EPOLL_CTL_MOD;
        struct epoll_event event, *event_p = &event;
        /* if *next is the only handler in this handler queue,
         * then delete corresponding fd from epoll instance */
        if(!(*channel)->handlers.slh_first)
        {
            op = EPOLL_CTL_DEL;
            event.data.ptr = NULL;
            tmq_map_erase(loop->channels, handler->fd);
            free(*channel);
        }
        /* otherwise, there are other handlers associated with this fd,
         * using EPOLL_CTL_MOD to modify events */
        else
        {
            (*channel)->all_events &= ~handler->events;
            event.events = (*channel)->all_events;
            event.data.ptr = *channel;
        }
        if(epoll_ctl(loop->epoll_fd, op, handler->fd, event_p) < 0)
            fatal_error("epoll_ctl() error %d: %s", errno, strerror(errno));
        tmq_vec_push_back(loop->removing_handlers, handler);
    }
    else tlog_warn("handler already unregistered");
}

int tmq_handler_is_registered(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    if(!loop || !handler) return 0;
    epoll_channel_t** channel = tmq_map_get(loop->channels, handler->fd);
    if(!channel) return 0;
    int registered = 0;
    tmq_event_handler_t* h;
    SLIST_FOREACH(h, &(*channel)->handlers, next_handler)
    {
        if(h == handler)
        {
            registered = 1;
            break;
        }
    }
    return registered;
}

tmq_timer_id_t tmq_event_loop_add_timer(tmq_event_loop_t* loop, tmq_timer_t* timer)
{
    if(!loop || !timer) return invalid_timer_id();
    return tmq_timer_heap_add(&loop->timer_heap, timer);
}

void tmq_event_loop_cancel_timer(tmq_event_loop_t* loop, tmq_timer_id_t timer_id)
{
    if(!loop) return;
    tmq_cancel_timer(&loop->timer_heap, timer_id);
}

int tmq_event_loop_resume_timer(tmq_event_loop_t* loop, tmq_timer_id_t timer_id)
{
    if(!loop) -1;
    return tmq_resume_timer(&loop->timer_heap, timer_id);
}

void tmq_event_loop_quit(tmq_event_loop_t* loop)
{
    atomicSet(loop->quit, 1);
    tmq_notifier_notify(&loop->quit_notifier);
}

void tmq_event_loop_destroy(tmq_event_loop_t* loop)
{
    if(atomicGet(loop->running))
        return;
    tmq_vec_free(loop->epoll_events);
    tmq_vec_free(loop->active_handlers);
    tmq_map_iter_t channel_it = tmq_map_iter(loop->channels);
    for(; tmq_map_has_next(channel_it); tmq_map_next(loop->channels, channel_it))
    {
        epoll_channel_t** channel = channel_it.second;
        free(*channel);
    }
    tmq_map_free(loop->channels);
    close(loop->epoll_fd);
    tmq_timer_heap_destroy(&loop->timer_heap);
}

static void tmq_notifier_on_notify(tmq_socket_t fd, uint32_t events, void* arg)
{
    int buf[65535];
    ssize_t n = read(fd, buf, sizeof(buf));
    if(n < 0)
        fatal_error("read() error %d: %s", errno, strerror(errno));
    const tmq_notifier_t* notifier = arg;
    if(notifier->cb)
        notifier->cb(notifier->arg);
}

void tmq_notifier_init(tmq_notifier_t* notifier, tmq_event_loop_t* loop, tmq_notify_cb cb, void* arg)
{
    if(!notifier || !loop) return;
    if(pipe2(notifier->wakeup_pipe, O_CLOEXEC | O_NONBLOCK) < 0)
        fatal_error("pipe2() error %d: %s", errno, strerror(errno));

    notifier->loop = loop;
    notifier->cb = cb;
    notifier->arg = arg;
    notifier->wakeup_handler = tmq_event_handler_new(notifier->wakeup_pipe[0], EPOLLIN,
                                                     tmq_notifier_on_notify, notifier, 0);
    tmq_handler_register(loop, notifier->wakeup_handler);
}

void tmq_notifier_notify(tmq_notifier_t* notifier)
{
    int wakeup = 1;
    write(notifier->wakeup_pipe[1], &wakeup, sizeof(wakeup));
}

void tmq_notifier_destroy(tmq_notifier_t* notifier)
{
    if(!notifier) return;
    tmq_handler_unregister(notifier->loop, notifier->wakeup_handler);
    close(notifier->wakeup_pipe[0]);
    close(notifier->wakeup_pipe[1]);
}
