
/*---------------------------------------------
    clmem.c
    -Implementation of a thread-safe queue
----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"
#include "../common.h"

typedef struct queue_node_{
    struct queue_node_ * next;
    void * data;
}queue_node;

struct queue_{
    pthread_mutex_t lock;
    pthread_cond_t trigger;
    queue_node * first, * last;
    unsigned n_elements;
    int terminate;
};

void queue_terminate(queue *q){
    pthread_mutex_lock(&q->lock);
    q->terminate = 1;
    pthread_mutex_unlock(&q->lock);
    pthread_cond_signal(&q->trigger);
}

queue * queue_create(){

    queue * retval = malloc(sizeof(queue));
    ASSERT_RETV(retval != NULL,NULL,"Could not allocate memmory for queue.");

    memset(retval,0,sizeof(queue));

    /*Init required mutexes and conditions*/
    if(pthread_mutex_init(&retval->lock,NULL) != 0){
        SHOW_ERROR("Could not create lock for queue mutual exclusion.");
        free(retval);
        return NULL;
    }
    
    if(pthread_cond_init(&retval->trigger,NULL) != 0){
        SHOW_ERROR("Could not create condition trigger for queue mutual exclusion.");
        pthread_mutex_destroy(&retval->lock);
        free(retval);
        return NULL;
    }

    return retval;
}

void queue_destroy(queue *q){
	pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->trigger);
    free(q);
}

int queue_push(queue * q, void * data){
    queue_node * new;
    
    new = malloc(sizeof(queue_node));
    ASSERT_RETV(new != NULL,1,"Could not allocate memmory for queue node.");

    new->next = NULL;
    new->data = data;

    pthread_mutex_lock(&q->lock);

    /*If a terminate signal is on, no new data shall be placed in queue*/
    if(q == NULL || q->terminate){
        SHOW_WARNING("Can not push data to queue: finished.");
        free(new);
        return -1;
    }
    
    if(q->last){
        q->last->next = new;
    }
    q->last = new;

    if(q->first == NULL){
        q->first = new;
    }
    q->n_elements++;
    pthread_mutex_unlock(&q->lock);

    /*Something new on the queue, send a signal*/
    pthread_cond_signal(&q->trigger);
    return 0;
}

void * queue_pop(queue * q){
    void * retval;
    queue_node * aux;

    pthread_mutex_lock(&q->lock);

    ASSERT_RETV(q != NULL,NULL,"Queue not initialized.");

    /*If nothing is available on queue, block until 
     *new data arrives*/
    while(q->n_elements == 0 && !q->terminate){
        pthread_cond_wait(&q->trigger,&q->lock);
    }

    /*Terminate signal triggered while waiting*/
    if(q->n_elements == 0 && q->terminate){
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    /*pop from queue*/
    aux = q->first;
    q->first = q->first->next;
    if(aux == q->last){
        q->last = aux->next;
    }
    q->n_elements--;

    pthread_mutex_unlock(&q->lock);

    retval = aux->data;
    free(aux);
    return retval;
}
