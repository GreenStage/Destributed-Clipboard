
#ifndef IF_HEADER
#define IF_HEADER

#define MAX_CLIPBOARDS 20
#define MAX_APPS 20

#define ERR_IF_EXISTS 1
#define ERR_LOCK_CREATE 2
#define ERR_CONNECT 3

void time_m_sync(unsigned now);
unsigned time_m_now();
unsigned time_m_local();

int clipif_init(int socket);
void *clipif_listen(void * socket);
int clipif_add_broadcast(void *data,int from);
void clipif_finalize();

int appif_init();
void *appif_listen(void * socket);
void appif_finalize();

#endif