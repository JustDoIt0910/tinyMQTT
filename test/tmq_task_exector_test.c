//
// Created by just do it on 2023/8/23.
//
#include "mqtt/mqtt_task_executor.h"
#include "tlog.h"
#include <stdlib.h>
#include <unistd.h>
#include "base/mqtt_util.h"

tmq_executor_t e;

int cnt = 0;

static void pri0_task(void* arg)
{
    tlog_info("priority 0 task %d\n", *(int*)arg);
    free(arg);
    usleep(1000 * 5);
}

static void pri1_task(void* arg)
{
    tlog_info("priority 1 task %d\n", *(int*)arg);
    free(arg);
    usleep(1000 * 10);
}

//static void pri2_task(void* arg)
//{
//    tlog_info("priority 2 task %d\n", *(int*)arg);
//    free(arg);
//    usleep(1000 * 100);
//}

static void* producer_routine2_(void* _)
{
    sleep(2);

    int k = 10;
    while(k--)
    {
        for(int i = 0; i < 10; i++)
        {
            int* arg = malloc(sizeof(int));
            *arg = i + 1;

            tmq_executor_post(&e, pri0_task, arg, 0);
        }
        sleep(1);
    }

    return NULL;
}

static void* producer_routine1_(void* _)
{
    sleep(1);

    int k = 10;
    while(k--)
    {
        for(int i = 0; i < 100; i++)
        {
            int* arg = malloc(sizeof(int));
            *arg = incrementAndGet(cnt, 1);

            tmq_executor_post(&e, pri1_task, arg, 1);
            usleep(100);
        }
        sleep(1);
    }

    return NULL;
}

//static void* producer_routine3_(void* _)
//{
//    sleep(3);
//
//    for(int i = 0; i < 100; i++)
//    {
//        int* arg = malloc(sizeof(int));
//        *arg = i;
//        tmq_task_t t = {
//                .exec = pri2_task,
//                .arg = arg
//        };
//        tmq_executor_post(&e, &t, 2);
//    }
//
//    return NULL;
//}

void main()
{
    tlog_init("test.log", 1024 * 1024, 10, 0, TLOG_SCREEN);

    tmq_executor_init(&e, 3);

    pthread_t p1, p2, p3;
    //pthread_create(&p1, NULL, producer_routine1_, NULL);
    pthread_create(&p2, NULL, producer_routine1_, NULL);
    pthread_create(&p3, NULL, producer_routine2_, NULL);

    tmq_executor_run(&e);

    while(1);

    tlog_exit();
}