#ifndef MEM_HEADER
#define MEM_HEADER

#define ERR_MEM_ALREADY_INIT -1
#define ERR_LOCK_CREATE -2

#ifdef VERBOSE
#define CLMEM_MSG(...)\
do {  \
    pthread_mutex_lock(&print_lock);\
    fprintf(stdout,"\x1B[32m[MEM]: \x1B[0m%s: ",__func__);\
    fprintf(stdout,__VA_ARGS__);\
    fprintf(stdout,"\n");\
    fflush(stderr);\
    pthread_mutex_unlock(&print_lock);\
} while(0)
#else
#define CLMEM_MSG(...) do{} while(0)
#endif

/*Initializes clipboard memory*/
int mem_init();

/*Gets data from region, up to size*/
unsigned mem_get(int region,void * buffer, unsigned size);

/*Waits for a data update on a region and returns the new data, up to size*/
unsigned mem_wait(int region,void * buffer, unsigned size);

/*Places data on a region (or not) depending on the update_cond value, up to size*/
unsigned mem_put(int region, void * data, unsigned size, unsigned update_cond);

/*Destroys the clipboard memory*/
void mem_finish();

void mem_destroy();
#endif
