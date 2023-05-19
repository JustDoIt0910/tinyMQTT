//
// Created by zr on 23-4-14.
//
#include <stdio.h>
#include "base/mqtt_map.h"

struct Value
{
    int a;
    int b;
};

typedef tmq_map(char*, struct Value) map_str_struct;

char* keys[] = {"key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9", "key10",
                "key11", "key12", "key13", "key14", "key15", "key16", "key17"};
struct Value values[] = {{12, 34}, {56, 78}, {32, 32}, {83, 46}, {15, 99},
                         {75, 27}, {82, 36}, {82, 37}, {10, 29}, {83, 9},
                         {23, 54}, {2, 5}, {67, 23}, {54, 23}, {5, 21},
                         {12, 38}, {23, 98}};


void get(map_str_struct* m, char* key)
{
    struct Value* res = tmq_map_get(*m, key);
    if(res)
        printf("%s => {%d, %d}\n", key, res->a, res->b);
}

int main()
{
    map_str_struct m;
    tmq_map_str_init(&m, struct Value, MAP_DEFAULT_CAP, MAP_DEFAULT_LOAD_FACTOR);

    for(int i = 0; i < 17; i++)
        tmq_map_put(m, keys[i], values[i]);

    for(int i = 0; i < 17; i++)
        get(&m, keys[i]);

    tmq_map_erase(m, "key2");
    printf("\n");

    for(int i = 0; i < 17; i++)
        get(&m, keys[i]);
    printf("\n");

    tmq_map_iter_t it;
    for(it = tmq_map_iter(m); tmq_map_has_next(it); tmq_map_next(m, it))
    {
        struct Value* res = it.entry->value;
        char* key = it.entry->key;
        printf("%s => {%d, %d}\n", key, res->a, res->b);
    }

    tmq_map_free(m);
    return 0;
}