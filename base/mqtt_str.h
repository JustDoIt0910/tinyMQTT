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

tmq_str_t tmq_str_new_len(const char* str, size_t size);
tmq_str_t tmq_str_new(const char* str);
tmq_str_t tmq_str_empty();
size_t tmq_str_len(tmq_str_t s);
tmq_str_t tmq_str_append_char(tmq_str_t s, char c);
tmq_str_t tmq_str_append_str(tmq_str_t s, const char* str);
tmq_str_t tmq_str_append_data_n(tmq_str_t s, const char* data, size_t n);
void tmq_str_clear(tmq_str_t s);
tmq_str_t tmq_str_assign(tmq_str_t s, const char* str);
tmq_str_t tmq_str_assign_n(tmq_str_t s, const char* str, size_t n);
void tmq_str_free(tmq_str_t s);
void tmq_str_debug(tmq_str_t s);
tmq_str_t tmq_str_parse_int(int v, int base);
char tmq_str_at(tmq_str_t s, size_t index);
tmq_str_t tmq_str_substr(tmq_str_t s, size_t start, size_t len);

#endif //TINYMQTT_MQTT_STR_H
