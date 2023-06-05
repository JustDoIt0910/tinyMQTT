//
// Created by zr on 23-6-5.
//
#include "mqtt/mqtt_topic.h"
#include <stdio.h>

void on_match(tmq_broker_t* broker, char* client_id, uint8_t qos, tmq_message* message)
{
    printf("(%s) => <%s, %u>\n", message->message, client_id, qos);
}

int main()
{
    tmq_topics_t topics;
    tmq_topics_init(&topics, NULL, on_match);

    tmq_topics_add_subscription(&topics, "test/topic/+/1", "client1", 0);

    tmq_message message = {
            .message = tmq_str_new("message"),
            .qos = 1
    };
    tmq_topics_match(&topics, 0, "test/topic/a/1", &message);
}