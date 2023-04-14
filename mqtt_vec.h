//
// Created by zr on 23-4-14.
//

#ifndef TINYMQTT_MQTT_VEC_H
#define TINYMQTT_MQTT_VEC_H
#include <stddef.h>

typedef struct tmq_vec_base_s
{
    size_t cap;
    size_t size;
    void* data;
    size_t elem_size;
} tmq_vec_base_t;

#define tmq_vec(T)          \
struct                      \
{                           \
    tmq_vec_base_t* base;   \
    T tmp_elem;             \
    T* elem_ref;            \
}

#define tmq_vec_init(T) \
{.base = tmq_base_init_(sizeof(T))}

#define tmq_vec_push_back(v, elem) \
((v).tmp_elem = (elem), tmq_vec_push_back_((v).base, &(v).tmp_elem))

#define tmq_vec_insert(v, index, elem) \
((v).tmp_elem = (elem), tmq_vec_insert_((v).base, index, &(v).tmp_elem))

#define tmq_vec_erase(v, index) tmq_vec_erase_((v).base, index)

#define tmq_vec_at(v, index) \
((v).elem_ref = tmq_vec_get_((v).base, index))

#define tmq_vec_begin(v) \
((v).elem_ref = tmq_vec_begin_((v).base))

#define tmq_vec_end(v) \
((v).elem_ref = tmq_vec_end_((v).base))

#define tmq_vec_clear(v) tmq_vec_clear_((v).base)
#define tmq_vec_free(v) tmq_vec_free_((v).base)
#define tmq_vec_size(v) tmq_vec_size_((v).base)
#define tmq_vec_empty(v) tmq_vec_empty_((v).base)

tmq_vec_base_t* tmq_base_init_(size_t elem_size);
int tmq_vec_push_back_(tmq_vec_base_t* v, const void* elem);
void* tmq_vec_get_(tmq_vec_base_t* v, int index);
void* tmq_vec_begin_(tmq_vec_base_t* v);
void* tmq_vec_end_(tmq_vec_base_t* v);
int tmq_vec_insert_(tmq_vec_base_t* v, int index, const void* elem);
int tmq_vec_erase_(tmq_vec_base_t* v, int index);
void tmq_vec_clear_(tmq_vec_base_t* v);
void tmq_vec_free_(tmq_vec_base_t* v);
size_t tmq_vec_size_(tmq_vec_base_t* v);
int tmq_vec_empty_(tmq_vec_base_t* v);

#endif //TINYMQTT_MQTT_VEC_H
