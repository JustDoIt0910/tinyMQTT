//
// Created by zr on 23-4-9.
//

#ifndef TINYMQTT_MQTT_MAP_H
#define TINYMQTT_MQTT_MAP_H
#include <stddef.h>
#include <stdint.h>

#define MAP_DEFAULT_CAP             0
#define MAP_DEFAULT_LOAD_FACTOR     75

typedef struct tmq_map_entry_s
{
    unsigned hash;
    void* key;
    void* value;
    struct tmq_map_entry_s* next;
    /* char key[]: memory for key */
    /* char value[]: memory for value */
} tmq_map_entry_t;

typedef unsigned(*tmq_map_hash_f)(const void*);
typedef int(*tmq_map_equal_f)(const void*, const void*);

enum tmq_map_key_type
{
    KEY_TYPE_BUILTIN,
    KEY_TYPE_STR,
    KEY_TYPE_CUSTOM
};

typedef struct tmq_map_base_s
{
    tmq_map_hash_f hash_fn;
    tmq_map_equal_f equal_fn;
    /* memory required to store a key */
    size_t key_size;
    /* memory required to store a value */
    size_t value_size;
    tmq_map_entry_t** buckets;
    uint32_t cap;
    uint32_t size;
    uint32_t load_fac;
    uint32_t remap_thresh;
    unsigned char key_type;
} tmq_map_base_t;

#define tmq_map(K, V)       \
struct                      \
{                           \
    tmq_map_base_t* base;   \
    K tmp_k;                \
    V tmp_v;                \
    V* res;                 \
}

#define tmq_map_str(V, cap, f) \
{.base = tmq_map_new_(cap, f, 0, sizeof(V), KEY_TYPE_STR, hash_str, equal_str)}

#define tmq_map_builtin32(V, cap, f) \
{.base = tmq_map_new_(cap, f, 4, sizeof(V), KEY_TYPE_BUILTIN, hash_32, equal_32)}

#define tmq_map_builtin64(V, cap, f) \
{.base = tmq_map_new_(cap, f, 8, sizeof(V), KEY_TYPE_BUILTIN, hash_64, equal_64)}

#define tmq_map_custom(K, V, cap, f, hash_f, equal_f) \
{.base = tmq_map_new_(cap, f, sizeof(K), sizeof(V), KEY_TYPE_CUSTOM, hash_f, equal_f)}


#define tmq_map_put(m, k, v) \
    (m).tmp_k = k;           \
    (m).tmp_v = v;           \
    tmq_map_put_((m).base, &((m).tmp_k), &((m).tmp_v))

#define tmq_map_get(m, k) \
((m).tmp_k = (k), (m).res = tmq_map_get_((m).base, &((m).tmp_k)))

tmq_map_base_t* tmq_map_new_(uint32_t cap, uint32_t factor,
                        size_t key_size, size_t value_size, unsigned char key_type,
                        tmq_map_hash_f hash_fn, tmq_map_equal_f equal_fn);
int tmq_map_put_(tmq_map_base_t* m, const void* key, const void* value);
void* tmq_map_get_(tmq_map_base_t* m, const void* key);
void tmq_map_free_(tmq_map_base_t* m);

unsigned hash_str(const void* key);
int equal_str(const void* k1, const void* k2);

unsigned hash_32(const void* key);
int equal_32(const void* k1, const void* k2);

unsigned hash_64(const void* key);
int equal_64(const void* k1, const void* k2);

#endif //TINYMQTT_MQTT_MAP_H
