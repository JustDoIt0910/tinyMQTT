//
// Created by zr on 23-4-15.
//

#ifndef TINYMQTT_MQTT_TCP_CONN_H
#define TINYMQTT_MQTT_TCP_CONN_H
#include "base/mqtt_socket.h"
#include "mqtt_buffer.h"
#include "event/mqtt_event.h"
#include "mqtt/mqtt_codec.h"
#include <pthread.h>

typedef struct tmq_io_group_s tmq_io_group_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef void(*tcp_close_cb)(tmq_tcp_conn_t* conn, void* arg);
typedef void(*context_cleanup_cb)(void* context);

typedef enum tmq_tcp_conn_state_e
{
   CONNECTED,
   DISCONNECTING,
   DISCONNECTED
} tmq_tcp_conn_state;

typedef struct tmq_tcp_conn_s
{
    tmq_socket_t fd;
    tmq_tcp_conn_state state;
    int ref_cnt;
    tmq_event_loop_t* loop;
    tmq_io_group_t* group;
    tmq_codec_t* codec;

    tmq_socket_addr_t local_addr, peer_addr;
    tmq_buffer_t in_buffer, out_buffer;

    tmq_event_handler_t* read_event_handler,
    *write_event_handler, *error_close_handler;
    int is_writing;

    tcp_close_cb close_cb;
    void* close_cb_arg;

    context_cleanup_cb ctx_clean_up;
    void* context;
} tmq_tcp_conn_t;

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_io_group_t* group,
                                 tmq_socket_t fd, tmq_codec_t* codec);
tmq_tcp_conn_t* get_ref(tmq_tcp_conn_t* conn);
void release_ref(tmq_tcp_conn_t* conn);

/* functions below must be called only in the io thread of the connection */
void tmq_tcp_conn_write(tmq_tcp_conn_t* conn, char* data, size_t size);
void tmq_tcp_conn_close(tmq_tcp_conn_t* conn);
int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size);
void tmq_tcp_conn_set_context(tmq_tcp_conn_t* conn, void* ctx, context_cleanup_cb cleanup_cb);
void tmq_tcp_conn_free(tmq_tcp_conn_t* conn);

#endif //TINYMQTT_MQTT_TCP_CONN_H
