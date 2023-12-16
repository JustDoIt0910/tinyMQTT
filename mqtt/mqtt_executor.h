//
// Created by just do it on 2023/8/24.
//

#ifndef TINYMQTT_MQTT_EXECUTOR_H
#define TINYMQTT_MQTT_EXECUTOR_H
#include <pthread.h>
#include "base/mqtt_vec.h"

typedef struct tmq_task_s
{
    void(*exec)(void* arg);
    void* arg;
} tmq_task_t;

typedef tmq_vec(tmq_task_t) task_buffer;

typedef struct task_exec_ctx_s
{
    task_buffer main_buf, aux_buf;
    size_t break_point;
}task_exec_ctx;

typedef struct tmq_executor_s
{
    task_exec_ctx* exec;

    pthread_t th;
    pthread_mutex_t lk;
    pthread_cond_t cond;

    int executing_pri;
    int preempt;
    int stop;

    int max_pri;
    size_t task_total;
}tmq_executor_t;

void tmq_executor_init(tmq_executor_t* executor, int pri_num);
void tmq_executor_run(tmq_executor_t* executor);
void tmq_executor_stop(tmq_executor_t* executor);
void tmq_executor_post(tmq_executor_t* executor, void(*routine)(void*), void* arg, int pri);

#endif //TINYMQTT_MQTT_EXECUTOR_H
