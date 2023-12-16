//
// Created by zr on 23-6-2.
//

#ifndef TINYMQTT_MQTT_IO_CONTEXT_H
#define TINYMQTT_MQTT_IO_CONTEXT_H
#include "event/mqtt_event.h"
#include "mqtt_types.h"

typedef void* tmq_mail_t;
typedef tmq_vec(tmq_mail_t) tmq_mail_list_t;
typedef struct tmq_mailbox_s tmq_mailbox_t;
typedef void(*mail_handler)(void* owner, tmq_mail_t mail);

typedef struct tmq_mailbox_s {
    void* owner;
    mail_handler handler;
    tmq_notifier_t notifier;
    pthread_mutex_t lk;
    tmq_mail_list_t mailbox;
} tmq_mailbox_t;

void tmq_mailbox_init(tmq_mailbox_t* mailbox, tmq_event_loop_t* loop,
                      void* owner, mail_handler handler);
void tmq_mailbox_push(tmq_mailbox_t* mailbox, tmq_mail_t mail);
void tmq_mailbox_destroy(tmq_mailbox_t* mailbox);

typedef tmq_map(char*, tmq_tcp_conn_t*) tcp_conn_map_t;
typedef struct tmq_io_context_s
{
    tmq_broker_t* broker;
    pthread_t io_thread;
    int index;
    tmq_event_loop_t loop;
    tcp_conn_map_t tcp_conns;
    tmq_timer_id_t mqtt_keepalive_timer;
    pthread_mutex_t stop_lk;
    pthread_cond_t stop_cond;

    tmq_mailbox_t pending_tcp_connections;
    tmq_mailbox_t mqtt_connect_responses;
    tmq_mailbox_t packet_sending_tasks;
    tmq_mailbox_t broadcast_tasks;
} tmq_io_context_t;

void tmq_io_context_init(tmq_io_context_t* context, tmq_broker_t* broker, int index);
void tmq_io_context_run(tmq_io_context_t* context);
void tmq_io_context_stop(tmq_io_context_t* context);
void tmq_io_context_destroy(tmq_io_context_t* context);

#endif //TINYMQTT_MQTT_IO_CONTEXT_H
