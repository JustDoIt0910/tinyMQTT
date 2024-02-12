//
// Created by just do it on 2024/2/12.
//

#ifndef TINYMQTT_MQTT_DELAY_MESSAGE_ADAPTOR_H
#define TINYMQTT_MQTT_DELAY_MESSAGE_ADAPTOR_H
#include "mqtt_adaptors.h"
#include <rabbitmq-c/amqp.h>

typedef struct
{
    ADAPTOR_PUBLIC_MEMBER
    amqp_socket_t* socket;
    amqp_connection_state_t conn;
    void* broker;
    void(*message_cb)(void*, tmq_str_t);
} delay_message_adaptor;

#endif //TINYMQTT_MQTT_DELAY_MESSAGE_ADAPTOR_H
