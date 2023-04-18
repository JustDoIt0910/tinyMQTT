//
// Created by zr on 23-4-14.
//
#include "mqtt_vec.h"
#include <stdio.h>

typedef tmq_vec(int) vec_int_t;

void print(vec_int_t* v)
{
    for(int* e = tmq_vec_begin(*v); e != tmq_vec_end(*v); e++)
        printf("%d ", *e);
    printf("\n");
}

struct Test
{
    char a;
    short b;
    int c;
    char* str;
};

struct Test elems[] = {{12, 23, 13, "elem1"}, {43, 23, 75, "elem2"}, {54, 2, 767, "elem3"},
                       {76, 34, 23, "elem4"}, {43, 231, 7, "elem5"}, {2, 53, 65, "elem6"},
                       {38, 27, 10, "elem7"}};

int main()
{
//    vec_int_t v = tmq_vec_init(int);
//
//    tmq_vec_push_back(v, 1);
//    tmq_vec_push_back(v, 2);
//    tmq_vec_push_back(v, 3);
//    tmq_vec_push_back(v, 4);
//    print(&v);
//
//    tmq_vec_insert(v, 0, 5);
//    print(&v);
//
//    tmq_vec_insert(v, tmq_vec_size(v), 6);
//    print(&v);
//
//    tmq_vec_erase(v, tmq_vec_size(v) - 1);
//    print(&v);
//
//    tmq_vec_erase(v, 3);
//    print(&v);
//
//    tmq_vec_free(v);

    tmq_vec(struct Test) v = tmq_vec_make(struct Test);

    tmq_vec_resize(v, 10);
    for(int i = 0; i < 6; i++)
        tmq_vec_push_back(v, elems[i]);
    tmq_vec_set(v, 0, elems[6]);

    for(struct Test* e = tmq_vec_begin(v); e != tmq_vec_end(v); e++)
        printf("{a = %d, b = %d, c = %d, str = %s}\n", e->a, e->b, e->c, e->str);

    tmq_vec_free(v);
    return 0;
}