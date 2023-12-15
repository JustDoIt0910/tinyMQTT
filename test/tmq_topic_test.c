//
// Created by zr on 23-6-5.
//
#include "mqtt/mqtt_topic.h"
#include <stdio.h>

void on_match(tmq_broker_t* broker, char* client_id,
              char* topic, uint8_t required_qos, tmq_message* message)
{
    printf("(%s) => <%s, %u>\n", message->message, client_id, required_qos);
}

int main()
{
    tmq_topics_t topics;
    tmq_topics_init(&topics, NULL, on_match);

    tmq_topics_add_subscription(&topics, "test/topic/+/1", "client1", 0);
    tmq_topics_add_subscription(&topics, "test/topic/1/+", "client2", 0);
    tmq_topics_add_subscription(&topics, "test/topic", "client3", 0);
    tmq_topics_add_subscription(&topics, "test/topic", "client5", 1);
    tmq_topics_add_subscription(&topics, "test/#", "client4", 0);


    tmq_message message = {
            .message = tmq_str_new("message"),
            .qos = 1
    };
    tmq_topics_publish(&topics, "test/topic", &message, 1);
    tmq_topics_info(&topics);
}