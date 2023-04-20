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

void timer_heap_insert(tmq_timer_heap_t* timer_heap, tmq_timer_t* timer)
{
    if(!timer_heap || !timer) return;
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
    timer_heap->heap[++timer_heap->size] = timer;
    timer_swim(timer_heap, timer_heap->size);
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

}

void tmq_timer_heap_init(tmq_timer_heap_t* timer_heap, tmq_event_loop_t* loop)
{
    if(!timer_heap) return;
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
    tmq_event_handler_t* handler = tmq_event_handler_create(timer_heap->timer_fd, EPOLLIN,
                                                            timer_heap_timeout, timer_heap);
    tmq_event_loop_register(loop, handler);
}