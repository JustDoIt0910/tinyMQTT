//
// Created by just do it on 2024/2/6.
//

#ifndef TINYMQTT_MQTT_ADAPTORS_H
#define TINYMQTT_MQTT_ADAPTORS_H
#include "base/mqtt_map.h"
#include "base/mqtt_str.h"
#include "base/mqtt_config.h"
#include <stdbool.h>

typedef enum {PARAMETER_STR, PARAMETER_INTEGER, PARAMETER_BOOL} adaptor_value_type;

typedef struct
{
    adaptor_value_type value_type;
    union
    {
        tmq_str_t str;
        int64_t integer;
        bool boolean;
    };
} adaptor_value_t;

typedef tmq_map(char*, adaptor_value_type) adaptor_parameter_map;
typedef tmq_map(char*, adaptor_value_t) adaptor_value_map;
typedef void(*register_parameters_f)(adaptor_parameter_map*);
typedef void(*handle_event_f)(adaptor_value_map* parameters, adaptor_value_map* payload);

#define add_parameter(m, name, type)   tmq_map_put((m), name, type)

#define ADAPTOR_PUBLIC_MEMBER                           \
register_parameters_f register_parameters;     \
handle_event_f handle_event;

typedef struct tmq_adaptor_s
{
    ADAPTOR_PUBLIC_MEMBER
} tmq_adaptor_t;

typedef tmq_adaptor_t*(*adaptor_getter_f)(tmq_config_t*);

#endif //TINYMQTT_MQTT_ADAPTORS_H
