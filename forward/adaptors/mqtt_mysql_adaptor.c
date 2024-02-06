//
// Created by just do it on 2024/2/6.
//
#include "mqtt_adaptors.h"
#include <stdlib.h>

static void register_parameters(adaptor_parameter_map* parameter_map)
{
    tmq_map_put(*parameter_map, "table", PARAMETER_STR);
}

static void handle_event(adaptor_value_map* parameters, adaptor_value_map* payload)
{

}

typedef struct
{
    ADAPTOR_PUBLIC_MEMBER
} mysql_adaptor;

tmq_adaptor_t* get_mysql_adaptor(tmq_config_t* cfg)
{
    mysql_adaptor* adaptor = malloc(sizeof(mysql_adaptor));
    adaptor->register_parameters = register_parameters;
    adaptor->handle_event = handle_event;
    return (tmq_adaptor_t*)adaptor;
}