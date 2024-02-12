//
// Created by just do it on 2024/2/12.
//
#include "mqtt_adaptors.h"
#include <stdlib.h>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include <cjson/cJSON.h>
#include <pthread.h>

typedef struct
{
    ADAPTOR_PUBLIC_MEMBER
    amqp_socket_t *socket;
    amqp_connection_state_t conn;
    void* broker;
    void(*message_cb)(void*, tmq_str_t);
} delay_message_adaptor;

static void register_parameters(adaptor_parameter_map* parameter_map)
{
    add_parameter(parameter_map, "exchange", ADAPTOR_VALUE_STR);
    add_parameter(parameter_map, "routingKey", ADAPTOR_VALUE_STR);
    add_parameter(parameter_map, "delayMS", ADAPTOR_VALUE_INTEGER);
}

static void handle_event(tmq_adaptor_t* adaptor, adaptor_value_map* parameters, adaptor_value_list* payload_kvs)
{
    delay_message_adaptor* rabbitmq = (delay_message_adaptor*)adaptor;
    char* exchange, *routing_key;
    adaptor_value_t* exchange_value = tmq_map_get(*parameters, "exchange");
    if(!exchange_value || exchange_value->value_type != ADAPTOR_VALUE_STR)
        return;
    exchange = exchange_value->str;

    adaptor_value_t* routing_key_value = tmq_map_get(*parameters, "routingKey");
    if(routing_key_value)
    {
        if(routing_key_value->value_type != ADAPTOR_VALUE_STR)
            return;
        routing_key = routing_key_value->str;
    }
    else routing_key = "";

    adaptor_value_t* delay_value = tmq_map_get(*parameters, "delayMS");
    if(!delay_value || delay_value->value_type != ADAPTOR_VALUE_INTEGER)
        return;
    int delay = delay_value->integer;

    cJSON* payload = cJSON_CreateObject();
    for(adaptor_value_item_t* it = tmq_vec_begin(*payload_kvs); it != tmq_vec_end(*payload_kvs); it++)
    {
        if(it->value.value_type == ADAPTOR_VALUE_STR)
            cJSON_AddStringToObject(payload, it->value_name, it->value.str);
        else
            cJSON_AddNumberToObject(payload, it->value_name, it->value.integer);
    }

    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG |
            AMQP_BASIC_EXPIRATION_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; /* persistent delivery mode */
    tmq_str_t ex = tmq_str_parse_int(delay, 10);
    if(!ex)
        return;
    props.expiration = amqp_cstring_bytes(ex);
    tmq_str_free(ex);
    char* body = cJSON_Print(payload);
    if(!body)
    {
        cJSON_Delete(payload);
        return;
    }
    amqp_basic_publish(rabbitmq->conn, 1, amqp_cstring_bytes(exchange),
                       amqp_cstring_bytes(routing_key), 0, 0,
                       &props, amqp_cstring_bytes(body));
    free(body);
    cJSON_Delete(payload);
}

static void* listen_routine(void* arg)
{
    delay_message_adaptor* adaptor = arg;
    while(1)
    {
        amqp_rpc_reply_t res;
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(adaptor->conn);
        res = amqp_consume_message(adaptor->conn, &envelope, NULL, 0);
        if (AMQP_RESPONSE_NORMAL != res.reply_type)
            continue;
        amqp_basic_ack(adaptor->conn, 2, envelope.delivery_tag, true);
        adaptor->message_cb(adaptor->broker, tmq_str_new_len(envelope.message.body.bytes,
                                                             envelope.message.body.len));
        amqp_destroy_envelope(&envelope);
    }
}

tmq_adaptor_t* get_delay_message_adaptor(tmq_config_t* cfg, tmq_str_t* error)
{
    tmq_str_t host = NULL;
    tmq_str_t port_s = NULL;
    tmq_str_t vhost = NULL;
    tmq_str_t username = NULL;
    tmq_str_t password = NULL;
    delay_message_adaptor* adaptor = NULL;
    host = tmq_config_get(cfg, "rabbitmq_ip");
    if(!host)
    {
        *error = tmq_str_new("rabbitmq ip configuration is missing");
        goto end;
    }
    port_s = tmq_config_get(cfg, "rabbitmq_port");
    if(!port_s)
    {
        *error = tmq_str_new("rabbitmq port configuration is missing");
        goto end;
    }
    amqp_socket_t *socket = NULL;
    amqp_connection_state_t conn;

    conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        *error = tmq_str_new("create tcp socket error");
        goto end;
    }
    int64_t port;
    tmq_str_to_int(port_s, &port);
    if (amqp_socket_open(socket, host, port)) {
        *error = tmq_str_new("open tcp socket error");
        goto end;
    }
    vhost = tmq_config_get(cfg, "rabbitmq_vhost");
    if(!vhost)
    {
        *error = tmq_str_new("rabbitmq vhost configuration is missing");
        goto end;
    }
    username = tmq_config_get(cfg, "rabbitmq_username");
    password = tmq_config_get(cfg, "rabbitmq_password");
    if(!username || !password)
    {
        *error = tmq_str_new("rabbitmq username/password configuration is missing");
        goto end;
    }
    amqp_rpc_reply_t reply = amqp_login(conn, vhost, 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN,
                                        username, password);
    if(reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        *error = tmq_str_new("rabbitmq login failed");
        goto end;
    }
    amqp_channel_open(conn, 1);
    amqp_channel_open(conn, 2);
    reply = amqp_get_rpc_reply(conn);
    if(reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        *error = tmq_str_new("create channel failed");
        goto end;
    }
    amqp_basic_consume(conn, 2, amqp_cstring_bytes("dead_letter_queue"), amqp_empty_bytes,
                       0, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(conn);
    if(reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        *error = tmq_str_new("amqp_basic_consume() error");
        goto end;
    }
    adaptor = malloc(sizeof(delay_message_adaptor));
    adaptor->register_parameters = register_parameters;
    adaptor->handle_event = handle_event;
    adaptor->socket = socket;
    adaptor->conn = conn;

    pthread_t listen_th;
    pthread_create(&listen_th, NULL, listen_routine, adaptor);
    pthread_detach(listen_th);

    end:
    tmq_str_free(host);
    tmq_str_free(port_s);
    tmq_str_free(vhost);
    tmq_str_free(username);
    tmq_str_free(password);
    return (tmq_adaptor_t*)adaptor;
}

void set_message_callback(tmq_adaptor_t* adaptor, void* broker, void(*message_cb)(void*, tmq_str_t))
{
    delay_message_adaptor* delay_message = (delay_message_adaptor*)adaptor;
    delay_message->broker = broker;
    delay_message->message_cb = message_cb;
}