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

tmq_event_handler_t* tmq_event_handler_new(int fd, short events, tmq_event_cb cb, void* arg)
{
    tmq_event_handler_t* handler = malloc(sizeof(tmq_event_handler_t));
    if(!handler)
        fatal_error("malloc() error: out of memory");
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
    tmq_map_64_init(&loop->removing_handlers, int, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_map_32_init(&loop->handler_map, epoll_handler_ctx, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);
    tmq_vec_resize(loop->epoll_events, INITIAL_EVENTLIST_SIZE);

    loop->running = 0;
    loop->quit = 0;
    loop->event_handling = 0;
    pthread_mutexattr_t attr;
    memset(&attr, 0, sizeof(pthread_mutexattr_t));
    if(pthread_mutexattr_init(&attr))
        fatal_error("pthread_mutexattr_init() error %d: %s", errno, strerror(errno));

    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    if(pthread_mutex_init(&loop->lk, &attr))
        fatal_error("pthread_mutex_init() error %d: %s", errno, strerror(errno));

    tmq_timer_heap_init(&loop->timer_heap, loop);
}

void tmq_event_loop_run(tmq_event_loop_t* loop)
{
    if(!loop) return;
    if(atomicExchange(loop->running, 1) == 1)
        return;
    pthread_mutex_lock(&loop->lk);
    while(!loop->quit)
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
                epoll_handler_ctx * ctx = tmq_map_get(loop->handler_map, event->data.fd);
                if(!ctx)
                    continue;
                tmq_event_handler_t* handler;
                SLIST_FOREACH(handler, &ctx->handlers, event_next)
                {
                    if(!(handler->events & event->events))
                        continue;
                    handler->r_events = event->events;
                    tmq_vec_push_back(loop->active_handlers, handler);
                }
            }
            loop->event_handling = 1;
            tmq_event_handler_t** it;
            for(it = tmq_vec_begin(loop->active_handlers);
                it != tmq_vec_end(loop->active_handlers);
                it++)
            {
                tmq_event_handler_t* active_handler = *it;
                /* It is possible that an active handler is unregistered by another active handler,
                 * in this case, it should not be accessed anymore because it may already be freed */
                if(tmq_map_get(loop->removing_handlers, active_handler) == NULL && active_handler->cb)
                    active_handler->cb(active_handler->fd, active_handler->r_events, active_handler->arg);
            }
            tmq_map_clear(loop->removing_handlers);
            loop->event_handling = 0;
        }
        else if(events_num < 0)
            tlog_error("epoll_wait() error %d: %s", errno, strerror(errno));
    }
    atomicSet(loop->running, 0);
}

void tmq_handler_register(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    if(!loop || !handler) return;
    struct epoll_event event;
    bzero(&event, sizeof(struct epoll_event));
    event.data.fd = handler->fd;
    event.events = handler->events;

    pthread_mutex_lock(&loop->lk);
    int op = EPOLL_CTL_ADD;
    epoll_handler_ctx* handler_ctx = tmq_map_get(loop->handler_map, handler->fd);
    if(!handler_ctx)
    {
        epoll_handler_ctx ctx;
        bzero(&ctx, sizeof(ctx));
        tmq_map_put(loop->handler_map, handler->fd, ctx);
        handler_ctx = tmq_map_get(loop->handler_map, handler->fd);
    }
    else
    {
        op = EPOLL_CTL_MOD;
        event.events = handler_ctx->all_events | handler->events;
    }
    assert(handler_ctx != NULL);
    SLIST_INSERT_HEAD(&handler_ctx->handlers, handler, event_next);
    handler_ctx->all_events = event.events;

    if(epoll_ctl(loop->epoll_fd, op, handler->fd, &event) < 0)
        fatal_error("epoll_ctl() error %d: %s", errno, strerror(errno));

    pthread_mutex_unlock(&loop->lk);
}

void tmq_handler_unregister(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    if(!loop || !handler) return;
    pthread_mutex_lock(&loop->lk);
    epoll_handler_ctx * ctx = tmq_map_get(loop->handler_map, handler->fd);
    if(!ctx)
    {
        tlog_warn("handler already unregistered");
        goto unlock;
    }
    tmq_event_handler_t** next = &ctx->handlers.slh_first;
    while(*next && *next != handler)
        next = &(*next)->event_next.sle_next;
    if(*next)
    {
        int op = EPOLL_CTL_MOD;
        struct epoll_event event, *event_p = &event;
        /* if *next is the only handler in this handler queue,
         * then delete corresponding fd from epoll instance */
        if(*next == ctx->handlers.slh_first && !(*next)->event_next.sle_next)
        {
            op = EPOLL_CTL_DEL;
            /* Since Linux 2.6.9, event can be specified as NULL when using EPOLL_CTL_DEL. */
            event_p = NULL;
            tmq_map_erase(loop->handler_map, handler->fd);
        }
        /* otherwise, there are other handlers associated with this fd,
         * using EPOLL_CTL_MOD to modify events */
        else
        {
            event.data.fd = handler->fd;
            ctx->all_events &= ~handler->events;
            event.events = ctx->all_events;
            *next = (*next)->event_next.sle_next;
        }
        if(epoll_ctl(loop->epoll_fd, op, handler->fd, event_p) < 0)
            fatal_error("epoll_ctl() error %d: %s", errno, strerror(errno));

        handler->event_next.sle_next = NULL;
        handler->cb = handler->arg = NULL;
        if(loop->event_handling)
            tmq_map_put(loop->removing_handlers, handler, 1);
    }
    else
        tlog_warn("handler already unregistered");
    unlock:
    pthread_mutex_unlock(&loop->lk);
}

int tmq_handler_is_registered(tmq_event_loop_t* loop, tmq_event_handler_t* handler)
{
    if(!loop || !handler) return 0;
    pthread_mutex_lock(&loop->lk);
    epoll_handler_ctx * ctx = tmq_map_get(loop->handler_map, handler->fd);
    if(!ctx)
    {
        pthread_mutex_unlock(&loop->lk);
        return 0;
    }
    int registered = 0;
    tmq_event_handler_t* h;
    SLIST_FOREACH(h, &ctx->handlers, event_next)
    {
        if(h == handler)
        {
            registered = 1;
            break;
        }
    }
    pthread_mutex_unlock(&loop->lk);
    return registered;
}

tmq_timerid_t tmq_event_loop_add_timer(tmq_event_loop_t* loop, tmq_timer_t* timer)
{
    if(!loop || !timer) return invalid_timerid();
    return tmq_timer_heap_add(&loop->timer_heap, timer);
}

void tmq_event_loop_cancel_timer(tmq_event_loop_t* loop, tmq_timerid_t timerid)
{
    if(!loop) return;
    tmq_cancel_timer(&loop->timer_heap, timerid);
}

void tmq_event_loop_quit(tmq_event_loop_t* loop) {atomicSet(loop->quit, 1);}

void tmq_event_loop_destroy(tmq_event_loop_t* loop)
{
    if(atomicGet(loop->running))
        return;
    tmq_vec_free(loop->epoll_events);
    tmq_vec_free(loop->active_handlers);
    tmq_map_iter_t it = tmq_map_iter(loop->handler_map);
    for(; tmq_map_has_next(it); tmq_map_next(loop->handler_map, it))
    {
        epoll_handler_ctx * ctx = it.second;
        tmq_event_handler_t* handler, *next;
        handler = ctx->handlers.slh_first;
        while(handler)
        {
            next = handler->event_next.sle_next;
            free(handler);
            handler = next;
        }
    }
    tmq_map_free(loop->handler_map);
    close(loop->epoll_fd);
    tmq_timer_heap_free(&loop->timer_heap);
}

static void tmq_notifier_on_notify(tmq_socket_t fd, uint32_t events, const void* arg)
{
    int buf[128];
    ssize_t n = read(fd, buf, sizeof(buf));
    if(n < 0)
        fatal_error("read() error %d: %s", errno, strerror(errno));
    const tmq_notifier_t* notifier = arg;
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
                                                     tmq_notifier_on_notify, notifier);
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
    free(notifier->wakeup_handler);
    close(notifier->wakeup_pipe[0]);
    close(notifier->wakeup_pipe[1]);
}