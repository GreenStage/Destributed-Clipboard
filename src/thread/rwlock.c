
/*---------------------------------------------
    rwlock.c
    -Implementation of Read/Write locks, with 
     an update trigger
----------------------------------------------*/

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "rwlock.h"

/*Rw lock structure*/
struct rw_lock_{
    pthread_mutex_t m_lock;
    pthread_cond_t update;
    sem_t sem;
    unsigned n_readers;
};

int rw_lock_init(rw_lock *lock){
    struct rw_lock_ * newlock;
    int ret;
    
    newlock = malloc(sizeof(struct rw_lock_));
    newlock->n_readers = 0;
    
    *lock = newlock;
    
    if(newlock == NULL){
        return -1;
    }
    else if((ret = pthread_mutex_init(&newlock->m_lock,NULL)) != 0){
        return ret;
    }
    else if((ret = pthread_cond_init(&newlock->update,NULL)) != 0){
        return ret;
    }
    /*Linux still uses sem_init*/
#ifdef __linux__
    else return sem_init(&newlock->sem,0,1);
#else
    else return 0;
#endif
}

int rw_lock_destroy(rw_lock lock){
    int ret;
    if(lock == NULL){
        return -1;
    }
    /*Linux still uses sem_destroy*/
#ifdef __linux__
    else if( (ret = sem_destroy(&lock->sem)) != 0){
        return -2;
    }
#endif
    else if( (ret = pthread_mutex_destroy(&lock->m_lock)) != 0){
        return -2;
    }
    else if( (ret = pthread_cond_destroy(&lock->update)) != 0){
        return -3;
    }
    free(lock);
    return 0;
}

int rw_lock_rlock(rw_lock lock){
    if(lock == NULL){
        return -1;
    }
    pthread_mutex_lock(&lock->m_lock);
    lock->n_readers++;
    if(lock->n_readers == 1){
        /*If is the first reader, wait for the semaphore*/
        sem_wait(&lock->sem);
    }
    pthread_mutex_unlock(&lock->m_lock);
    return 0;
}

int rw_lock_wlock(rw_lock lock){
    if(lock == NULL){
        return -1;
    }
    /*Wait for the semaphore*/
    sem_wait(&lock->sem);
    return 0;
}

int rw_lock_runlock(rw_lock lock){
    if(lock == NULL){
        return -1;
    }
    pthread_mutex_lock(&lock->m_lock);
    lock->n_readers--;
    if(lock->n_readers == 0){
        /*If is the last reader, release for the semaphore*/
        sem_post(&lock->sem);
    }
    pthread_mutex_unlock(&lock->m_lock); 
    return 0;
}

int rw_lock_wunlock(rw_lock lock){
    if(lock == NULL){
        return -1;
    }
    /*Increment the semaphore counter*/
    sem_post(&lock->sem);

    /*Broadcast update signal*/
    pthread_cond_broadcast(&lock->update);
    return 0;
}

int rw_lock_wait_update(rw_lock lock){
    if(lock == NULL){
        return -1;
    }
    return pthread_cond_wait(&lock->update,&lock->m_lock);
}
