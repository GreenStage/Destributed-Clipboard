#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "rwlock.h"

struct rw_lock_{
    pthread_mutex_t m_lock;
    pthread_cond_t update;
    sem_t sem;
    unsigned n_readers;
};

int rw_lock_init(rw_lock lock){
    int ret;
    lock = malloc(sizeof(struct rw_lock_));
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
    else if((ret = pthread_mutex_init(&lock->m_lock,NULL)) != 0){
        return ret;
    }
    else if((ret = pthread_cond_init(&lock->update,NULL)) != 0){
        return ret;
    }
#ifdef __linux__
    else return sem_init(&lock->sem,0,1);
#else
    else return 0;
#endif
}

int rw_lock_destroy(rw_lock lock){
    int ret;
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
#ifdef __linux__
    else if( (ret = sem_destroy(&lock->sem) != 0)){
        return ret;
    }
#endif
    else if( (ret = pthread_mutex_destroy(&lock->m_lock)) != 0){
        return ret;
    }
    else if( (ret = pthread_cond_destroy(&lock->update)) != 0){
        return ret;
    }
    free(lock);
    return 0;
}

int rw_lock_rlock(rw_lock lock){
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
    pthread_mutex_lock(&lock->m_lock);
    lock->n_readers++;
    if(lock->n_readers == 1){
        sem_wait(&lock->sem);
    }
    pthread_cond_broadcast(&lock->update);
    pthread_mutex_unlock(&lock->m_lock);
    return 0;
}

int rw_lock_wlock(rw_lock lock){
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
    sem_wait(&lock->sem);
    return 0;
}

int rw_lock_runlock(rw_lock lock){
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
    pthread_mutex_lock(&lock->m_lock);
    lock->n_readers--;
    if(lock->n_readers == 0){
        sem_post(&lock->sem);
    }
    pthread_mutex_unlock(&lock->m_lock); 
    return 0;
}

int rw_lock_wunlock(rw_lock lock){
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
    sem_post(&lock->sem);
    return 0;
}

int rw_lock_wait_update(rw_lock lock){
    if(lock == NULL){
        return ERR_LOCK_NULL;
    }
    return pthread_cond_wait(&lock->update,&lock->m_lock);
}