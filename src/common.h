
#ifndef COMMON_HEADER
#define COMMON_HEADER

#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define SHOW_ERRORS
#define DEBUG 
#define ERROR_MESSAGE_SIZE 400
#define N_REGIONS 10
#define ERR_NO_IP 1
#define ERR_INVALID_IP 2
#define ERR_NO_PORT 3
#define ERR_INV_PORT 4
#define ERR_CANT_CONNECT 5
#define ERR_SOCKET 6
#define ERR_BIND_SOCKET 7
#define ERR_MEM_ALLOC 8
#define ERR_MUTEX_CREATE 9

extern pthread_mutex_t print_lock;

#define MIN(A,B) (A) > (B) ? (B) : (A)

#ifdef SHOW_ERRORS
#define SHOW_ERROR(...) \
do {  \
    pthread_mutex_lock(&print_lock);\
    fprintf(stderr,"\x1B[31m[ERROR]: \x1B[0m%s: ",__func__);\
    fprintf(stderr,__VA_ARGS__);\
    fprintf(stderr,"\n");\
    pthread_mutex_unlock(&print_lock);\
} while(0)

#else
#define SHOW_ERROR(t,r,...) do{} while(0)
#endif

#ifdef DEBUG

#define SHOW_INFO(...)\
do {  \
    pthread_mutex_lock(&print_lock);\
    fprintf(stdout,"\x1B[34m[INFO]: \x1B[0m%s: ",__func__);\
    fprintf(stdout,__VA_ARGS__);\
    fprintf(stdout,"\n");\
    pthread_mutex_unlock(&print_lock);\
} while(0)

#define SHOW_WARNING(...)\
do {  \
    pthread_mutex_lock(&print_lock);\
    fprintf(stdout,"\x1B[33m[WARNING]: \x1B[0m%s: ",__func__);\
    fprintf(stdout,__VA_ARGS__);\
    fprintf(stdout,"\n");\
    fflush(stderr);\
    pthread_mutex_unlock(&print_lock);\
} while(0)


#else
#define SHOW_INFO(t,r,...) do{} while(0)
#endif

#define ASSERT_RETV(A,R,...)\
do{\
    if(!(A)){\
        SHOW_ERROR(__VA_ARGS__);\
        return R;\
    }\
} while(0)

#define ASSERT_RET(A,...)\
do{\
    if(!(A)){\
        SHOW_ERROR(__VA_ARGS__);\
        return;\
    }\
} while(0)

#define CLOSE(a)\
do{\
    if(close(a) == -1){\
        pthread_mutex_lock(&print_lock);\
        SHOW_ERROR("Could not close socket %d: %s.",a,strerror(errno));\
        pthread_mutex_unlock(&print_lock);\
    }\
}while(0)

typedef enum bool_{
    false,
    true
} bool;

#endif