//
// Created by just do it on 2024/2/3.
//
#include "mqtt_adaptors.h"
#include <stdlib.h>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include <cjson/cJSON.h>

typedef struct
{
    ADAPTOR_PUBLIC_MEMBER
    amqp_socket_t *socket;
    amqp_connection_state_t conn;
} rabbitmq_adaptor;

static void register_parameters(adaptor_parameter_map* parameter_map)
{
    add_parameter(parameter_map, "exchange", ADAPTOR_VALUE_STR);
    add_parameter(parameter_map, "routingKey", ADAPTOR_VALUE_STR);
}

static void handle_event(tmq_adaptor_t* adaptor, adaptor_value_map* parameters, adaptor_value_list* payload_kvs)
{
    rabbitmq_adaptor* rabbitmq = (rabbitmq_adaptor*)adaptor;
    char* exchange, *routing_key;
    adaptor_value_t* exchange_value = tmq_map_get(*parameters, "exchange");
    if(exchange_value)
    {
        if(exchange_value->value_type != ADAPTOR_VALUE_STR)
            return;
        exchange = exchange_value->str;
    }
    else exchange = "";

    adaptor_value_t* routing_key_value = tmq_map_get(*parameters, "routingKey");
    if(routing_key_value)
    {
        if(routing_key_value->value_type != ADAPTOR_VALUE_STR)
            return;
        routing_key = routing_key_value->str;
    }
    else routing_key = "";

    cJSON* payload = cJSON_CreateObject();
    for(adaptor_value_item_t* it = tmq_vec_begin(*payload_kvs); it != tmq_vec_end(*payload_kvs); it++)
    {
        if(it->value.value_type == ADAPTOR_VALUE_STR)
            cJSON_AddStringToObject(payload, it->value_name, it->value.str);
        else
            cJSON_AddNumberToObject(payload, it->value_name, it->value.integer);
    }
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; /* persistent delivery mode */
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

tmq_adaptor_t* get_rabbitmq_adaptor(tmq_config_t* cfg, void* arg, tmq_str_t* error)
{
    tmq_str_t host = NULL;
    tmq_str_t port_s = NULL;
    tmq_str_t vhost = NULL;
    tmq_str_t username = NULL;
    tmq_str_t password = NULL;
    host = tmq_config_get(cfg, "rabbitmq_ip");
    if(!host)
    {
        *error = tmq_str_new("rabbitmq ip configuration is missing");
        goto failed;
    }
    port_s = tmq_config_get(cfg, "rabbitmq_port");
    if(!port_s)
    {
        *error = tmq_str_new("rabbitmq port configuration is missing");
        goto failed;
    }
    amqp_socket_t *socket = NULL;
    amqp_connection_state_t conn;

    conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        *error = tmq_str_new("create tcp socket error");
        goto failed;
    }
    int64_t port;
    tmq_str_to_int(port_s, &port);
    if (amqp_socket_open(socket, host, port)) {
        *error = tmq_str_new("open tcp socket error");
        goto failed;
    }
    vhost = tmq_config_get(cfg, "rabbitmq_vhost");
    if(!vhost)
    {
        *error = tmq_str_new("rabbitmq vhost configuration is missing");
        goto failed;
    }
    username = tmq_config_get(cfg, "rabbitmq_username");
    password = tmq_config_get(cfg, "rabbitmq_password");
    if(!username || !password)
    {
        *error = tmq_str_new("rabbitmq username/password configuration is missing");
        goto failed;
    }
    amqp_rpc_reply_t reply = amqp_login(conn, vhost, 0, AMQP_DEFAULT_FRAME_SIZE, 0, AMQP_SASL_METHOD_PLAIN,
                                        username, password);
    if(reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        *error = tmq_str_new("rabbitmq login failed");
        goto failed;
    }
    amqp_channel_open(conn, 1);
    reply = amqp_get_rpc_reply(conn);
    if(reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        *error = tmq_str_new("create channel failed");
        goto failed;
    }
    rabbitmq_adaptor* adaptor = malloc(sizeof(rabbitmq_adaptor));
    adaptor->register_parameters = register_parameters;
    adaptor->handle_event = handle_event;
    adaptor->socket = socket;
    adaptor->conn = conn;
    return (tmq_adaptor_t*)adaptor;

    failed:
    tmq_str_free(host);
    tmq_str_free(port_s);
    tmq_str_free(vhost);
    tmq_str_free(username);
    tmq_str_free(password);
    return NULL;
}