//
// Created by zr on 23-4-18.
//
#include "mqtt_timer.h"
#include "mqtt_event.h"
#include "tlog.h"
#include <stdlib.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

static int64_t time_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

static void timerfd_set_timeout(int timer_fd, int64_t when)
{
    int64_t now = time_now();
    int64_t timeout = when - now < 100 ? 100 : when - now;
    struct timespec spec;
    /* seconds */
    spec.tv_sec = timeout / 1000000;
    /* nanoseconds */
    spec.tv_nsec = (timeout % 1000000) * 1000;
    struct itimerspec it;
    bzero(&it, sizeof(it));
    it.it_value = spec;
    if(timerfd_settime(timer_fd, 0, &it, NULL) < 0)
    {
        tlog_fatal("timerfd_settime() error: %d: %s", errno, strerror(errno));
        tlog_exit();
        abort();
    }
}

tmq_timer_t* tmq_timer_new(double timeout_ms, int repeat, tmq_timer_cb cb, void* arg)
{
    if(timeout_ms < 0)
    {
        tlog_error("timeout or interval can't be nagetive");
        return NULL;
    }
    tmq_timer_t* timer = (tmq_timer_t*) malloc(sizeof(tmq_timer_t));
    if(!timer)
    {
        tlog_fatal("realloc() error: out of memory");
        tlog_exit();
        abort();
    }
    timer->timeout_ms = timeout_ms;
    timer->expire = time_now() + (int64_t) (timeout_ms * 1000);
    timer->repeat = repeat;
    timer->canceled = 0;
    timer->arg = arg;
    timer->cb = cb;
    return timer;
}

void tmq_timer_reset(tmq_timer_t* timer)
{
    timer->expire = time_now() + (int64_t) (timer->timeout_ms * 1000);
    timer->canceled = 0;
}

static void timer_swim(tmq_timer_heap_t* timer_heap, size_t idx)
{
    size_t p = PARENT_IDX(idx);
    tmq_timer_t* timer = timer_heap->heap[idx];
    for(; p > 0 && timer->expire < timer_heap->heap[p]->expire; idx = p, p = PARENT_IDX(idx))
        timer_heap->heap[idx] = timer_heap->heap[p];
    timer_heap->heap[idx] = timer;
}

static void timer_sink(tmq_timer_heap_t* timer_heap, size_t idx)
{
    if(timer_heap->size == 0)
        return;
    tmq_timer_t* timer = timer_heap->heap[idx];
    while(LEFT_CHILD_IDX(idx) <= timer_heap->size)
    {
        size_t child = LEFT_CHILD_IDX(idx);
        size_t right = RIGHT_CHILD_IDX(idx);
        if(right <= timer_heap->size &&
        timer_heap->heap[right]->expire < timer_heap->heap[child]->expire)
            child = right;
        if(timer_heap->heap[child]->expire < timer->expire)
            timer_heap->heap[idx] = timer_heap->heap[child];
        else break;
        idx = child;
    }
    timer_heap->heap[idx] = timer;
}

static int timer_heap_insert(tmq_timer_heap_t* timer_heap, tmq_timer_t* timer)
{
    if(!timer_heap || !timer) return 0;
    if(timer_heap->size == timer_heap->cap)
    {
       tmq_timer_t** heap = (tmq_timer_t**) realloc(timer_heap->heap, timer_heap->cap * 2 + 1);
       if(!heap)
       {
           tlog_fatal("realloc() error: out of memory");
           tlog_exit();
           abort();
       }
       timer_heap->heap = heap;
       timer_heap->cap = timer_heap->cap * 2;
    }
    tmq_timer_t* ori_top = timer_heap->size > 0 ? timer_heap->heap[1] : NULL;
    timer_heap->heap[++timer_heap->size] = timer;
    timer_swim(timer_heap, timer_heap->size);
    if(timer_heap->heap[1] != ori_top)
        return 1;
    return 0;
}

void tmq_timer_heap_add(tmq_timer_heap_t* timer_heap, tmq_timer_t* timer)
{
    if(timer_heap_insert(timer_heap, timer))
        timerfd_set_timeout(timer_heap->timer_fd, timer_heap->heap[1]->expire);
}

static tmq_timer_t* timer_heap_pop(tmq_timer_heap_t* timer_heap)
{
    tmq_timer_t* top = NULL;
    if(timer_heap->size < 1)
        return NULL;
    top = timer_heap->heap[1];
    timer_heap->heap[1] = timer_heap->heap[timer_heap->size--];
    if(timer_heap->size > 1)
        timer_sink(timer_heap, 1);
    return top;
}

static void timer_heap_timeout(int timer_fd, uint32_t event, const void* arg)
{
    int64_t now = time_now();
    uint64_t timeout_cnt;
    ssize_t n = read(timer_fd, &timeout_cnt, sizeof(timeout_cnt));
    if(n != sizeof(timeout_cnt))
        tlog_error("error reading timer_fd");
    tmq_timer_heap_t* timer_heap = (tmq_timer_heap_t*) arg;
    if(!timer_heap || timer_heap->size < 1)
        return;
    tmq_timer_t* top = timer_heap->heap[1];
    tmq_vec_clear(timer_heap->expired_timers);
    while(top->expire <= now)
    {
        timer_heap_pop(timer_heap);
        tmq_vec_push_back(timer_heap->expired_timers, top);
        if(timer_heap->size == 0)
            break;
        top = timer_heap->heap[1];
    }
    tmq_timer_t** timer = tmq_vec_begin(timer_heap->expired_timers);
    for(; timer != tmq_vec_end(timer_heap->expired_timers); timer++)
    {
        if((*timer)->canceled)
            continue;
        (*timer)->cb((*timer)->arg);
    }
    timer = tmq_vec_begin(timer_heap->expired_timers);
    for(; timer != tmq_vec_end(timer_heap->expired_timers); timer++)
    {
        if((*timer)->repeat && !(*timer)->canceled)
        {
            (*timer)->expire = now + (int64_t) ((*timer)->timeout_ms * 1000);
            timer_heap_insert(timer_heap, *timer);
        }
    }
    if(timer_heap->size > 0)
        timerfd_set_timeout(timer_heap->timer_fd, timer_heap->heap[1]->expire);
}

void tmq_timer_heap_init(tmq_timer_heap_t* timer_heap, tmq_event_loop_t* loop)
{
    if(!timer_heap || !loop) return;
    timer_heap->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(timer_heap->timer_fd < 0)
    {
        tlog_fatal("timerfd_create() error %d: %s", errno, strerror(errno));
        tlog_exit();
        abort();
    }
    timer_heap->heap = (tmq_timer_t**) malloc(sizeof(tmq_timer_t*) * TIMER_HEAP_INITIAL_SIZE + 1);
    if(!timer_heap->heap)
    {
        tlog_fatal("malloc() error: out of memory");
        tlog_exit();
        abort();
    }
    timer_heap->size = 0;
    timer_heap->cap = TIMER_HEAP_INITIAL_SIZE;
    tmq_vec_init(&timer_heap->expired_timers, tmq_timer_t*);
    tmq_event_handler_t* handler = tmq_event_handler_create(timer_heap->timer_fd, EPOLLIN,
                                                            timer_heap_timeout, timer_heap);
    tmq_event_loop_register(loop, handler);
}

void tmq_timer_heap_free(tmq_timer_heap_t* timer_heap)
{
    if(!timer_heap) return;
    if(timer_heap->heap)
        free(timer_heap->heap);
    tmq_vec_free(timer_heap->expired_timers);
    close(timer_heap->timer_fd);
}

void tmq_cancel_timer(tmq_timer_t* timer) {timer->canceled = 1;}

/* for debug */
static void tmq_timer_print(const tmq_timer_t* timer)
{
    printf("timer{exp=%lu, repeat=%d, cb=%p}\n", timer->expire, timer->repeat, timer->cb);
}

void tmq_timer_heap_print(tmq_timer_heap_t* timer_heap)
{
    if(!timer_heap) return;
    while(timer_heap->size > 0)
    {
        tmq_timer_t* timer = timer_heap_pop(timer_heap);
        tmq_timer_print(timer);
    }
}