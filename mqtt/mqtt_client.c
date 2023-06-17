//
// Created by zr on 23-4-9.
//
#include "mqtt_client.h"
#include <stdlib.h>
#include <string.h>

tiny_mqtt* tiny_mqtt_new(const char* ip, uint16_t port)
{
    tiny_mqtt* mqtt = malloc(sizeof(tiny_mqtt));
    if(!mqtt) return NULL;
    bzero(mqtt, sizeof(tiny_mqtt));
    tmq_event_loop_init(&mqtt->loop);
    tmq_codec_init(&mqtt->codec, CLIENT_CODEC);
}

void tiny_mqtt_loop(tiny_mqtt* mqtt)
{
    tmq_event_loop_run(&mqtt->loop);
    tmq_event_loop_destroy(&mqtt->loop);
}