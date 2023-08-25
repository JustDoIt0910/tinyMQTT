//
// Created by just do it on 2023/8/24.
//

#include <stdlib.h>
#include "tlog.h"
#include "base/mqtt_util.h"
#include "mqtt_task_executor.h"


void tmq_executor_init(tmq_executor_t* executor, int pri_num)
{
    executor->exec = malloc(pri_num * sizeof(task_exec_ctx));
    for(int i = 0; i < pri_num; i++)
    {
        task_exec_ctx* ctx = &executor->exec[i];
        ctx->break_point = 0;
        tmq_vec_init(&ctx->main_buf, tmq_task_t);
        tmq_vec_init(&ctx->aux_buf, tmq_task_t);
    }

    pthread_mutex_init(&executor->lk, NULL);
    pthread_cond_init(&executor->cond, NULL);

    executor->executing_pri = -1;
    executor->preempt = 0;
    executor->task_total = 0;
    executor->max_pri = pri_num - 1;
}

static void* thread_routine_(void* arg)
{
    tmq_executor_t* executor = arg;

    task_buffer tasks;
    size_t start;

    while(1)
    {
        pthread_mutex_lock(&executor->lk);
        while(executor->task_total == 0)
            pthread_cond_wait(&executor->cond, &executor->lk);

        task_exec_ctx* ctx;
        int i =  executor->max_pri;
        while(i >= 0)
        {
            ctx = &executor->exec[i];
            if(tmq_vec_size(ctx->main_buf) > 0 || tmq_vec_size(ctx->aux_buf) > 0)
                break;
            i--;
        }

        executor->executing_pri = i;
        executor->preempt = 0;

        tmq_vec_init(&tasks, tmq_task_t);
        if(tmq_vec_size(ctx->aux_buf) > 0)
        {
            start = ctx->break_point;
            tmq_vec_swap(tasks, ctx->aux_buf);
        }
        else
        {
            start = 0;
            tmq_vec_swap(tasks, ctx->main_buf);
        }

        executor->task_total -= (tmq_vec_size(tasks) - start);
        pthread_mutex_unlock(&executor->lk);

        for(size_t t = start; t < tmq_vec_size(tasks); t++)
        {
            if(atomicGet(executor->preempt))
            {
                pthread_mutex_lock(&executor->lk);
                executor->task_total += (tmq_vec_size(tasks) - t);
                tmq_vec_swap(tasks, ctx->aux_buf);
                ctx->break_point = t;
                pthread_mutex_unlock(&executor->lk);
                break;
            }
            tmq_task_t* task = tmq_vec_at(tasks, t);
            task->exec(task->arg);
        }
        tmq_vec_free(tasks);
    }
}

void tmq_executor_run(tmq_executor_t* executor)
{
    pthread_create(&executor->th, NULL, thread_routine_, executor);
}

void tmq_executor_post(tmq_executor_t* executor, void(*routine)(void*), void* arg, int pri)
{
    tmq_task_t task = {
            .exec = routine,
            .arg = arg
    };

    pthread_mutex_lock(&executor->lk);
    task_exec_ctx* ctx = &executor->exec[pri];
    tmq_vec_push_back(ctx->main_buf, task);
    executor->task_total++;
    if(executor->executing_pri < pri)
        atomicSet(executor->preempt, 1);
    pthread_mutex_unlock(&executor->lk);
    pthread_cond_signal(&executor->cond);
}