//
// Created by zr on 23-4-15.
//

#ifndef TINYMQTT_MQTT_TCP_CONN_H
#define TINYMQTT_MQTT_TCP_CONN_H
#include "mqtt_socket.h"
#include "mqtt_buffer.h"
#include "event/mqtt_event.h"
#include "codec/mqtt_codec.h"
#include <pthread.h>

typedef struct tmq_io_context_s tmq_io_context_t;
typedef struct tmq_tcp_conn_s tmq_tcp_conn_t;
typedef void(*tcp_close_cb)(tmq_tcp_conn_t* conn, void* arg);
typedef void(*context_cleanup_cb)(void* context);
typedef void(*write_complete_cb)(void* arg);

typedef enum tmq_tcp_conn_state_e
{
   CONNECTED,
   DISCONNECTING,
   DISCONNECTED
} tmq_tcp_conn_state;

typedef struct tmq_tcp_conn_s
{
    REF_COUNTED_MEMBERS
    tmq_socket_t fd;
    tmq_tcp_conn_state state;
    tmq_event_loop_t* loop;
    tmq_io_context_t* io_context;
    tmq_codec_t* codec;

    tmq_socket_addr_t local_addr, peer_addr;
    tmq_buffer_t in_buffer, out_buffer;

    tmq_event_handler_t* read_event_handler,
    *write_event_handler, *error_close_handler;
    int is_writing;

    tcp_close_cb on_close;
    write_complete_cb on_write_complete;
    void* cb_arg;

    context_cleanup_cb ctx_clean_up;
    void* context;
} tmq_tcp_conn_t;

#define TCP_CONN_SHARE(conn) ((tmq_tcp_conn_t*) get_ref((tmq_ref_counted_t*) (conn)))
#define TCP_CONN_RELEASE(conn) release_ref((tmq_ref_counted_t*) (conn))

tmq_tcp_conn_t* tmq_tcp_conn_new(tmq_event_loop_t* loop, tmq_io_context_t* io_context,
                                 tmq_socket_t fd, tmq_codec_t* codec);

/* functions below must be called only in the io thread of the connection */
void tmq_tcp_conn_write(tmq_tcp_conn_t* conn, char* data, size_t size);
void tmq_tcp_conn_shutdown(tmq_tcp_conn_t* conn);
void tmq_tcp_conn_force_close(tmq_tcp_conn_t* conn);
int tmq_tcp_conn_id(tmq_tcp_conn_t* conn, char* buf, size_t buf_size);
sa_family_t tmq_tcp_conn_family(tmq_tcp_conn_t* conn);
void tmq_tcp_conn_set_codec(tmq_tcp_conn_t* conn, tmq_codec_t* codec);
void tmq_tcp_conn_set_context(tmq_tcp_conn_t* conn, void* ctx, context_cleanup_cb cleanup_cb);
void tmq_tcp_conn_free(tmq_tcp_conn_t* conn);

#endif //TINYMQTT_MQTT_TCP_CONN_H
