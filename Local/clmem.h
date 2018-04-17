#ifndef MEM_HEADER
#define MEM_HEADER

#define ERR_MEM_ALREADY_INIT 1

#define CLMEM_MSG(...)\
do {  \
    pthread_mutex_lock(&print_lock);\
    fprintf(stdout,"\x1B[32m[MEM]: \x1B[0m%s: ",__func__);\
    fprintf(stdout,__VA_ARGS__);\
    fprintf(stdout,"\n");\
    fflush(stderr);\
    pthread_mutex_unlock(&print_lock);\
} while(0)

int mem_init();
unsigned mem_get(int region,void * buffer, unsigned size);
unsigned mem_wait(int region,void * buffer, unsigned size);
unsigned mem_put(int region, void * data, unsigned size, unsigned update_cond);
int mem_remove(int region);
void mem_finish();

#endif