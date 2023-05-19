//
// Created by zr on 23-5-19.
//

#ifndef TINYMQTT_MQTT_STR_H
#define TINYMQTT_MQTT_STR_H

#include <stddef.h>

#define TMQ_DS_HDR(s) (tmq_ds_t*)((char*)(s) - sizeof(tmq_ds_t))

struct __attribute__((__packed__)) tmq_ds_s
{
    size_t len;
    size_t alloc;
    char buf[];
};

typedef struct tmq_ds_s tmq_ds_t;
typedef char* tmq_str_t;

tmq_str_t tmq_str_new_len(const char*, size_t);
tmq_str_t tmq_str_new(const char*);
tmq_str_t tmq_str_empty();
size_t tmq_str_len(tmq_str_t);
tmq_str_t tmq_str_append_char(tmq_str_t, char);
tmq_str_t tmq_str_append_str(tmq_str_t, const char*);
void tmq_str_clear(tmq_str_t);
tmq_str_t tmq_str_assign(tmq_str_t, const char*);
void tmq_str_free(tmq_str_t);
void tmq_str_debug(tmq_str_t);
tmq_str_t tmq_str_parse_int(int, int);
char tmq_str_at(tmq_str_t, size_t);
tmq_str_t tmq_str_substr(tmq_str_t, size_t, size_t);

#endif //TINYMQTT_MQTT_STR_H
