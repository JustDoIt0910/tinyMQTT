//
// Created by just do it on 2024/1/11.
//

#ifndef TINYMQTT_MQTT_DB_H
#define TINYMQTT_MQTT_DB_H
#include <mongoc/mongoc.h>

typedef struct sending_packet_s sending_packet_t;

typedef struct tmq_db_return_receipt_s
{
    void(*receipt_routine)(void*);
    void* arg;
} tmq_db_return_receipt_t;

typedef struct sending_packet sending_packet;
void store_messages_to_mongodb(mongoc_client_t* mongo_client, const char* mqtt_client_id, sending_packet_t* packets,
                               int n_packets);
int fetch_messages_from_mongodb(mongoc_client_t* mongo_client, const char* mqtt_client_id, int limit,
                                sending_packet_t** result_head, sending_packet_t** result_tail);

#endif //TINYMQTT_MQTT_DB_H
