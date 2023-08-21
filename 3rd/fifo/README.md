# fifo
A first-in-first-out(FIFO) queue implemented in C, based on the linux kernel linked list. It is a practice of how to use list_head data structure, etc. 

## Usage 
   #define QueueElement TYPE

   Queue* q = initQueue(20);

   QueueElement e1,e2; 

   /* init e1, e2*/ 

   enqueue(q,e1);

   enqueue(q,e2);

   dequeue(q);

# Type generic issue
  This queue implementation is type generic using void* type. It needs to be downcast to a certain
  data type, which can be dangerous without proper type runtime check. 

  Alternatively, we can use Macro + structure offset to implement type generic queue data structures. Examples
   include:
  - list.h (kernel linked list), 
  - [uthash](http://troydhanson.github.io/uthash/) approach. 
  
  However, the usage is a bit complicated because 
  - it has certain assumptions about how to use the data structure, e.g., list_for_each_entry(tmp, &frame_queue->list, list). tmp needs to be the struct type contains the list member declaration. 
  - it needs to use the API calls to manipulate the data struct. That is, you need to memorize a bunch of functions, e.g.,  list_for_each_entry, HASH_FIND_INT(), etc. 

## Credits
   list.h is copied from Kulesh Shanmugasundaram's site: 
   http://isis.poly.edu/kulesh/stuff/src/klist/
   Kulesh deserves all the credits for posting the explanation and
the adapted source code. 