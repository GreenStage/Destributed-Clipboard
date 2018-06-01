#ifndef QUEUE_HEADER
#define QUEUE_HEADER

typedef struct queue_ queue;

/*Creates an empty queue*/
queue * queue_create();

/*Attempts to enqueue data at the end of the queue*/
int queue_push(queue * q, void * data);

/*Attempts to remove data from the front of the queue*/
void * queue_pop(queue * q);

/*Activates the termination signal, rejecting new data*/
void queue_terminate();

/*Destroys a queue*/
void queue_destroy(queue *q);

#endif
