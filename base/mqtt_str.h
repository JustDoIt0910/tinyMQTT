//
// Created by zr on 23-5-19.
//

#ifndef TINYMQTT_MQTT_STR_H
#define TINYMQTT_MQTT_STR_H

#include <stddef.h>
#include <sys/types.h>
#include "mqtt_vec.h"

#define TMQ_DS_HDR(s) (tmq_ds_t*)((char*)(s) - sizeof(tmq_ds_t))

struct __attribute__((__packed__)) tmq_ds_s
{
    size_t len;
    size_t alloc;
    char buf[];
};

typedef struct tmq_ds_s tmq_ds_t;
typedef char* tmq_str_t;
typedef tmq_vec(tmq_str_t) str_vec;

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
tmq_str_t tmq_str_parse_int(int64_t v, int base);
char tmq_str_at(tmq_str_t s, size_t index);
int tmq_str_equal(tmq_str_t s1, tmq_str_t s2);
int tmq_str_startswith(tmq_str_t s, const char* prefix);
tmq_str_t tmq_str_substr(tmq_str_t s, size_t start, size_t len);
ssize_t tmq_str_find(tmq_str_t s, char c);
str_vec tmq_str_split(tmq_str_t s, const char* delimeters);
void tmq_str_trim(tmq_str_t s);

#endif //TINYMQTT_MQTT_STR_H
