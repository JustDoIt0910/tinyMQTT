#include "queue.h"

/* Create a Queue */
Queue * initQueue()
{
    Queue *Q;
    Q = (Queue *)malloc(sizeof(Queue));
    Q->size = 0;
    /* initialize the list head. Kernel list method */
    INIT_LIST_HEAD(&Q->list);
    return Q;
}

/* pop out the queue front, and free the element space */
void dequeue(Queue *Q)
{
    Queue* tmp;
    if(Q->size==0){
	printf("Queue is Empty.\n");
	return;
    }else{
	Q->size--;
	tmp = list_entry(Q->list.next, Queue, list);
	list_del(Q->list.next);
	free(tmp);
    }
}

QueueElement front(Queue *Q)
{
    Queue* first_element;
    struct list_head * first;
    if(Q->size==0){
	printf("Queue is Empty\n");
	exit(0);
    }
    /* find the first element first */
    first = Q->list.next;
    /* reconstruct the first structure */
    first_element = list_entry(first, Queue, list);
    return first_element->e; 
}

QueueElement tail(Queue *Q)
{
    Queue* last_element;
    struct list_head * last;
    if(Q->size==0){
	printf("Queue is Empty.\n");
	exit(0);
    }
    /* find the last element first */
    last = Q->list.prev;
    /* reconstruct the last structure */
    last_element = list_entry(last, Queue, list);
    return last_element->e; 
}

size_t size(Queue* Q) {return Q->size;}

void enqueue(Queue *Q, QueueElement element)
{
    Queue* newQ;
	Q->size++;
	newQ = (Queue*) malloc(sizeof(Queue));
	newQ->e = element;
	/* add to the list tail */
	list_add_tail(&(newQ->list), &(Q->list));
}

/*
int test()
{
    int max = 20;
    Queue *testQueue = initQueue(max);
    enqueue(testQueue,1);
    enqueue(testQueue,2);
    printf("Front element is %d\n",front(testQueue));
    printf("Last element is %d\n",tail(testQueue));
    enqueue(testQueue,3);
    dequeue(testQueue);
    enqueue(testQueue,4);
    printf("Front element is %d\n", front(testQueue));
    printf("Last element is %d\n",tail(testQueue));
    dequeue(testQueue);
    dequeue(testQueue);
    printf("Front element is %d\n", front(testQueue));
    printf("Last element is %d\n",tail(testQueue));
}
*/

//int main(){
//  return 0;
//}
