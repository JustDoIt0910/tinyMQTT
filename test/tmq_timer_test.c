//
// Created by zr on 23-4-20.
//
#include "event/mqtt_timer.h"
#include "event/mqtt_event.h"
#include "tlog.h"
#include <stdio.h>

void timeout1(void* arg)
{
    tlog_info("timeout1");
}

void timeout2(void* arg)
{
    tlog_info("timeout2");
}

void timeout3(void* arg)
{
    tlog_info("timeout3");
}

int main()
{
    tlog_init("broker.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_event_loop_t loop;
    tmq_event_loop_init(&loop);

    tmq_timer_t* timer1 = tmq_timer_new(2000, 1, timeout1, NULL);
    tmq_timer_t* timer2 = tmq_timer_new(1000, 1, timeout2, NULL);
    tmq_timer_t* timer3 = tmq_timer_new(500, 1, timeout3, NULL);
    tmq_event_loop_add_timer(&loop, timer1);
    tmq_event_loop_add_timer(&loop, timer2);
    tmq_event_loop_add_timer(&loop, timer3);

    tmq_event_loop_run(&loop);
    tmq_event_loop_clean(&loop);
    tlog_exit();
    return 0;
}