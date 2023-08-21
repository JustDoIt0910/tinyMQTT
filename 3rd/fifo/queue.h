#include<stdio.h>
#include<stdlib.h>

#include "list.h"

/* by default, QueueElement is int
 Usage: #define QueueElement <TYPE> */
#ifndef QueueElement
#define QueueElement void*
#endif

typedef struct{
    size_t size;
    QueueElement e;
    struct list_head list;
} Queue;

Queue * initQueue();
QueueElement front(Queue *Q);	
QueueElement tail(Queue* Q);
void dequeue(Queue *Q);
void enqueue(Queue *Q, QueueElement element);
size_t size(Queue* Q);

