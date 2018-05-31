#ifndef QUEUE_HEADER
#define QUEUE_HEADER

typedef struct queue_ queue;

queue * queue_create();
int queue_push(queue * q, void * data);
void * queue_pop(queue * q);
void queue_destroy(queue *q);
void queue_terminate();
#endif