
#ifndef DCLIF_HEADER
#define DCLIF_HEADER

#define MAX_CLIPBOARDS 20
#define MAX_APPS 20

#define ERR_IF_EXISTS 1
#define ERR_LOCK_CREATE 2

#define CONN_TYPES 2

typedef enum{
    CONN_NONE = 0x0,
    CONN_CLIPBOARD,
    CONN_APPLICATION
} connection_type_t;


int dclif_init(int socket);
void *dclif_listen(void * socket);
void dclif_add_broadcast(void *data,int from);
void dclif_finalize();

int appif_init();
void *appif_listen(void * socket);
void appif_finalize();

int sendData(int sock,void * buf, int size);
#define recvData(sock,buf,size) recv(sock,buf,size,MSG_WAITALL)
#endif