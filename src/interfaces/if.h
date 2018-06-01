
#ifndef IF_HEADER
#define IF_HEADER

#define CLOCK_SYNC_PERIOD 30
#define MAX_CLIPBOARDS 20
#define MAX_APPS 20

/*Initiliazes the clipboards interface*/
int clipif_init(int socket);

/*Listens to clipboard connections*/
void *clipif_listen(void * socket);

/*Adds a message to be broadcasted to all clipboards except the one with the socket fd 
provided by "from" */
int clipif_add_broadcast(void *data,int from);

/*Terminates all connections with clipboards ands destroys the interface*/
void clipif_finalize();

/*Initiliazes the applications interface*/
int appif_init();

/*Listens to application connections*/
void *appif_listen(void * socket);

/*Terminates all connections with applications ands destroys the interface*/
void appif_finalize();

#endif
