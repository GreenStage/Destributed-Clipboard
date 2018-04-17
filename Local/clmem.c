#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "utils/time_m.h"
#include "common.h"
#include "clmem.h"

struct clipboard_memory{
    struct zones{
        void * data;
        unsigned size, from;
        pthread_mutex_t lock;
        pthread_cond_t trigger;
        unsigned update_cond;
    } zones[N_REGIONS];
} *mem;

int mem_init(){
    int i;

    ASSERT_RETV(mem == NULL,ERR_MEM_ALREADY_INIT,"Clipboard memmory already initialized.");

    mem = malloc(sizeof(struct clipboard_memory));
    ASSERT_RETV(mem != NULL,ERR_MEM_ALLOC,"Could not allocate clipboard memory.");

    memset(mem,0,sizeof(struct clipboard_memory));

    for(i = 0; i < N_REGIONS;i++){
        if(pthread_mutex_init(&mem->zones[i].lock,NULL) != 0){
            SHOW_ERROR("Could not create lock for mutual exclusion in region %d.",i);
            free(mem);
            mem = NULL;
            return ERR_MUTEX_CREATE;
        }
        else if(pthread_cond_init(&mem->zones[i].trigger,NULL) != 0){
            SHOW_ERROR("Could not create lock for mutual exclusion in region %d.",i);
            pthread_mutex_destroy(&mem->zones[i].lock);
            free(mem);
            mem = NULL;
            return ERR_MUTEX_CREATE;
        }
    }

    CLMEM_MSG("%d regions of clipboard memory ready.",N_REGIONS);

    return 1;
}

unsigned mem_wait(int region,void * buffer, unsigned size){
    unsigned cp_size, aux;

    ASSERT_RETV(region >= 0 && region < N_REGIONS,0,"Attempting to subscribe to memory region %d: Out ouf bounds.",region);

    pthread_mutex_lock(&mem->zones[region].lock);

    aux =   mem->zones[region].update_cond;
    while(aux == mem->zones[region].update_cond){
        pthread_cond_wait(&mem->zones[region].trigger,&mem->zones[region].lock);
    }
    
    cp_size = MIN(size,mem->zones[region].size);
    memcpy(buffer,mem->zones[region].data,cp_size);

    pthread_mutex_unlock(&mem->zones[region].lock);
    return cp_size;
}

unsigned mem_get(int region,void * buffer, unsigned size){
    unsigned cp_size;

    ASSERT_RETV(region >= 0 && region < N_REGIONS,0,"Attempting to get data from memory region %d: Out ouf bounds.",region);

    pthread_mutex_lock(&mem->zones[region].lock);

    cp_size = MIN(size,mem->zones[region].size);
    memcpy(buffer,mem->zones[region].data,cp_size);

    pthread_mutex_unlock(&mem->zones[region].lock);
    return cp_size;
}

unsigned mem_put(int region, void * data, unsigned size, unsigned update_cond){
    void * mem_cpy = NULL;
    unsigned retval;

    retval = time_m_now();
    ASSERT_RETV(region >= 0 && region < N_REGIONS,0,"Attempting to copy data to memory region %d: Out ouf bounds.",region);

    ASSERT_RETV(size > 0 ,0,"Attempting to copy empty data to memory region %d.",region);

    pthread_mutex_lock(&mem->zones[region].lock);

    if(mem->zones[region].data != NULL){
        if(mem->zones[region].update_cond > update_cond){
            return 0;
        }
        else{
            CLMEM_MSG("Removed %u bytes from region %d.",mem->zones[region].size,region);
            free(mem->zones[region].data);
        }

    }

    mem_cpy = malloc(size);
    memcpy(mem_cpy,data,size);

    mem->zones[region].data = mem_cpy;
    mem->zones[region].size = size;
    mem->zones[region].update_cond = retval;

    pthread_cond_broadcast(&mem->zones[region].trigger);
    pthread_mutex_unlock(&mem->zones[region].lock);

    CLMEM_MSG("Placed %u bytes in region %d.",size,region);
    return retval;
}

int mem_remove(int region){
    return mem_put(region,NULL,0,time_m_now());
}

void mem_finish(){
    int i;

    for(i = 0; i < N_REGIONS;i++){
        pthread_mutex_lock(&mem->zones[i].lock);
        if(mem->zones[i].data != NULL)
            free(mem->zones[i].data);
        mem->zones[i].data = NULL;
        pthread_mutex_unlock(&mem->zones[i].lock);
    }

    CLMEM_MSG("All %d memory regions freed.",N_REGIONS);
    free(mem);
}