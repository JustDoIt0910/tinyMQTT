//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_BROKER_H
#define TINYMQTT_MQTT_BROKER_H
#include "event/mqtt_event.h"
#include "net/mqtt_acceptor.h"
#include "base/mqtt_str.h"
#include "base/mqtt_map.h"
#include "base/mqtt_config.h"
#include "mqtt_codec.h"

#define MQTT_TCP_CHECKALIVE_INTERVAL    10
#define MQTT_CONNECT_MAX_PENDING        100
#define MQTT_TCP_MAX_IDLE               300
#define MQTT_IO_THREAD                  4

typedef struct tmq_broker_s tmq_broker_t;
typedef tmq_vec(tmq_packet_t) pending_packet_list;
typedef enum conn_state_e
{
    NO_SESSION,
    STARTING_SESSION,
    IN_SESSION
} conn_state_e;

typedef struct tcp_conn_ctx_s
{
    int64_t last_msg_time;
    union
    {
        tmq_broker_t* broker;
        tmq_session_t* session;
    } upstream;
    conn_state_e conn_state;
    pkt_parsing_ctx parsing_ctx;
    pending_packet_list pending_packets;
} tcp_conn_ctx;

typedef enum session_ctl_op_e
{
    SESSION_CONNECT,
    SESSION_DISCONNECT,
    SESSION_CLOSE
} session_ctl_op_e;
typedef struct session_connect_req
{
    tmq_connect_pkt connect_pkt;
    tmq_tcp_conn_t* conn;
} session_connect_req;

typedef struct session_ctl
{
    session_ctl_op_e op;
    union
    {
        session_connect_req start_req;
        tmq_session_t* session;
    } context;
} session_ctl;
typedef tmq_vec(session_ctl) session_ctl_list;

typedef struct session_connect_resp
{
    connack_return_code return_code;
    tmq_session_t* session;
    int session_present;
} session_connect_resp;
typedef tmq_vec(session_connect_resp) connect_resp_list;

typedef tmq_map(char*, tmq_tcp_conn_t*) tcp_conn_map_t;
typedef struct tmq_io_group_s
{
    tmq_broker_t* broker;
    pthread_t io_thread;
    tmq_event_loop_t loop;
    tcp_conn_map_t tcp_conns;
    tmq_timerid_t tcp_checkalive_timer;

    tmq_notifier_t new_conn_notifier;
    pthread_mutex_t pending_conns_lk;
    /* guarded by pending_conns_lk */
    tmq_vec(tmq_socket_t) pending_conns;

    pthread_mutex_t connect_resp_lk;
    /* guarded by connect_resp_lk */
    connect_resp_list connect_resp;
    tmq_notifier_t connect_resp_notifier;
} tmq_io_group_t;

typedef tmq_map(char*, tmq_session_t*) tmq_session_map;
typedef struct tmq_broker_s
{
    tmq_event_loop_t event_loop;
    tmq_acceptor_t acceptor;
    tmq_codec_t codec;
    int next_io_group;
    tmq_io_group_t io_groups[MQTT_IO_THREAD];
    tmq_config_t conf, pwd_conf;
    tmq_session_map sessions;

    pthread_mutex_t session_ctl_lk;
    /* guarded by session_ctl_lk */
    session_ctl_list session_ctl_reqs;
    tmq_notifier_t session_ctl_notifier;
} tmq_broker_t;

int tmq_broker_init(tmq_broker_t* broker, const char* cfg);
void tmq_broker_run(tmq_broker_t* broker);

#endif //TINYMQTT_MQTT_BROKER_H
