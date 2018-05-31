

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef __linux_
#include <linux/tcp.h>
#endif

#include "../common.h"
#include "../mem/clmem.h"
#include "../thread/queue.h"
#include "../utils/time.h"
#include "../utils/packet.h"
#include "if.h"

/*Single connection structure*/
typedef struct connection_{
        int sock_fd;
        pthread_t recv_thread;
        pthread_mutex_t lock;
} connection_t;

typedef struct distributed_interface_{
    int n_connections;
    unsigned parentDelay;
    connection_t connections[MAX_CLIPBOARDS];
    pthread_mutex_t lock;
    queue * broadcasts;
    pthread_t master_thread;
    
    int parentSock;
    pthread_t clock_thread;
} distributed_interface;

typedef struct broadcast_t_{
    struct packet * p;
    int from;
} broadcast_t;

distributed_interface *clipif = NULL;

/*-------------------------------
        Private functions
 -------------------------------*/

/*Thread callback function, in charge of generating clock sync packets*/
void *clipif_clock_man(){
    struct packet_sync p;
    p.packetType = PACKET_TIME_SYNC;

    while(1){
        sleep(CLOCK_SYNC_PERIOD);
        p.time = time_m_now();
        SHOW_INFO("Sending clock sync time to child clipboards: %u",p.time);
        clipif_add_broadcast(&p,-1);
    }
}

/*Clean up function called after exiting a slave thread*/
void clipif_close_conn(void * i){
    int index = *((int*)i);
    int socket;
    free(i);

    /*Access to a specific connection structure should be atomic*/
    pthread_mutex_lock(&clipif->connections[index].lock);
    socket = clipif->connections[index].sock_fd;
    clipif->connections[index].sock_fd = 0;
    pthread_mutex_unlock(&clipif->connections[index].lock);

    SHOW_INFO("Closing socket %d.",socket);
    CLOSE(socket);

    /*Make sure that access to clif->parentSock is atomic
      this will only happen once on the programm lifetime*/
    if(clipif->parentSock == socket){
        /*Connection with root clipboard was closed, clipboard should become
          root of his own tree*/
        clipif->parentSock = -1;
        SHOW_INFO("Connection with root clipboard closed, becoming root...");
        pthread_create(&clipif->clock_thread,NULL,clipif_clock_man,NULL);
    }

    pthread_mutex_lock(&clipif->lock);
    clipif->n_connections--;
    pthread_mutex_unlock(&clipif->lock);

}

/*-------------------------------
        Public functions
 -------------------------------*/

int clipif_add_broadcast(void *data,int from){
    int err;
    broadcast_t *broadcast;

    ASSERT_RETV(clipif != NULL,-1,"Distributed clipboards interface not initialized.");
    ASSERT_RETV(clipif->broadcasts != NULL,-2,"Broadcast queue not initialized.");

    broadcast = malloc(sizeof(broadcast_t));
    ASSERT_RETV(broadcast != NULL,-3,"Could not allocate memory for broadcast message.");

    broadcast->from = from;
    broadcast->p = (struct packet*) data;

    /*Ensure message was added to atomic queue*/
    if((err = queue_push(clipif->broadcasts,(void*) broadcast)) != 0){
        free(broadcast);
        return err;
    }
    else return 0;
}

void * clipif_slave(void * index){
    char buffer[sizeof(struct packet_data)], *dataBuffer;
    unsigned pBytes,pSize,pDataSize,pSyncSize;
    struct packet * p;

    long long err = 1;
    int sock,index_,withParent,err2;

    index_ = *((int*)index);

    /*No need to lock the connection the mutex here, since 
     *clipif->connections[index_] elements only change after exiting this thread*/
    sock = clipif->connections[index_].sock_fd;

    if(sock == clipif->parentSock){
        withParent = 1;
    }
    else withParent = 0;


    ASSERT_RETV(clipif != NULL,NULL,"Distributed clipboards interface not initialized.");

    /*In case thread gets cancelled by request,
      ensure clipif_close_conn iscalled anyway*/
    pthread_cleanup_push(clipif_close_conn,index);

    pSize = sizeof(struct packet);
    pDataSize = sizeof(struct packet_data);
    pSyncSize = sizeof(struct packet_sync);

    p = (struct packet*) &buffer;

    while(1) {
        if(err < 1) break;
        struct packet_data * pd = NULL;
        struct packet_sync * ps = NULL;

        /*Only allow cancellation to occur when waiting for messages,
          thus avoiding memory leaks*/
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        /*Read basic packet information*/
        if( (err = recvData(sock,p,pSize)) != pSize ){
            continue;
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        switch(p->packetType){

            case PACKET_TIME_SYNC:
                /*Only should listen to syncronization packets from parent*/
                if(withParent){
                    dataBuffer = malloc(pSyncSize);
                    ps = (struct packet_sync*) dataBuffer;

                    /*Read sync data*/
                    err = recvData(sock,dataBuffer + pSize,pSyncSize - pSize);
                    if(err != pSyncSize - pSize){
                        SHOW_WARNING("Invalid packet size %lli.",err);
                        break;
                    }

                    SHOW_INFO("Received clock sync: %u",ps->time);

                    /*Calculate parent's time taking into account the avg.
                     delay a message takes to arrive*/
                    time_m_sync(ps->time + clipif->parentDelay);

                    ps->packetType = PACKET_TIME_SYNC;
                    ps->time = time_m_now();

                    /*Forward sync packet to children clipboards*/
                    if( (err2 = clipif_add_broadcast(ps,sock)) != 0){
                        SHOW_ERROR("Could not broadcast message from clipboard %d: %d",sock,err2);
                        free(ps);
                    }
                }
                break;

            case PACKET_REQUEST_COPY:
                err = recvData(sock,p + pSize,pDataSize - pSize);
                if(err != pDataSize - pSize){
                    break;
                }

                pBytes = ((struct packet_data*) p)->dataSize;

                dataBuffer = malloc(pDataSize + pBytes);
                memcpy(dataBuffer,p,pDataSize);

                err = recvData(sock,dataBuffer + pDataSize,pBytes);
                if(err != pBytes){
                     SHOW_WARNING("Invalid packet %lli.",err);
                     free(pd);
                     break;
                }

                pd = (struct packet_data*) dataBuffer;
                pBytes = mem_put(pd->region,(void*) (dataBuffer + pDataSize),pBytes,pd->recv_at);
                if(pBytes == 0){
                    free(pd);
                }
                else{
                    pd->packetType = PACKET_REQUEST_COPY;
                    if( (err2 = clipif_add_broadcast(pd,sock)) != 0){
                        SHOW_ERROR("Could not broadcast message from clipboard %d: %d",sock,err2);
                        free(pd);
                    }
                }
                break;

            case PACKET_GOODBYE:
                err = 0;
                break;

            default:
                SHOW_WARNING("Invalid packet %d.",p->packetType);
                err = 0;
                break;
        }
    }
    if(err < 1){
        if(err == 0) SHOW_INFO("Connection closed with clipboard %d.",index_);
        else SHOW_WARNING("Error reading from socket with clipboard %d: %s.",index_,strerror(errno));

        SHOW_INFO("Connected clipboards: %d/%d", clipif->n_connections,MAX_CLIPBOARDS);
    }
    pthread_cleanup_pop(0);
    clipif_close_conn(index);
    return NULL;
}

void * clipif_broadcast(){
    broadcast_t * msg;

    ASSERT_RETV(clipif != NULL,NULL,"Distributed clipboard interface not initialized.");

    while( ( msg = (broadcast_t*) queue_pop(clipif->broadcasts) ) != NULL) {
        uint32_t size;
        switch(msg->p->packetType){
            case PACKET_GOODBYE:
                size = sizeof(struct packet);
                break;
            case PACKET_TIME_SYNC:
                size = sizeof(struct packet_sync);
                break;
            case PACKET_REQUEST_COPY:
                size = sizeof(struct packet_data) + ((struct packet_data*)msg->p)->dataSize;
                break;
            default:
                size = 0;
        }
        if(size != 0){
            for(int i = 0; i < MAX_CLIPBOARDS;i++){
                pthread_mutex_lock(&clipif->connections[i].lock);
                if(clipif->connections[i].sock_fd > 0 && (clipif->connections[i].sock_fd != msg->from )) {
                    sendData(clipif->connections[i].sock_fd,(void*) msg->p,size);
                }
                pthread_mutex_unlock(&clipif->connections[i].lock);
            }
            if(msg->from != -1)
                free(msg->p);
            free(msg);
        }
    }
    return NULL;
}

void *clipif_listen(void * socket){
    int sock;
    struct packet_sync p_hello;
    long long err;
    struct sockaddr_in new_client;
    unsigned addr_size;

    ASSERT_RETV(socket != NULL,NULL,"No socket passed.");
    sock =  *((int*)socket);
    free(socket);

    addr_size = sizeof(struct sockaddr_in);

    p_hello.packetType = PACKET_TIME_SYNC;

    ASSERT_RETV(clipif != NULL,NULL,"Distributed Clipboard Interface not initialized.");
    ASSERT_RETV(sock > 0,NULL,"Invalid socket %d.",sock);
    ASSERT_RETV(listen(sock,MAX_CLIPBOARDS) == 0,NULL,"Can not listen on socket: %s.",strerror(errno));

    SHOW_INFO("Started to listen to clipboards connection requests.");
    while(1){
        int new_client_fd;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        new_client_fd = accept(sock, (struct sockaddr *) &new_client,&addr_size);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        p_hello.time = time_m_now();

        pthread_mutex_lock(&clipif->lock);
        if(new_client_fd < 0){
            SHOW_WARNING("Could not accept new connection: invalid socket.");
        }
        else if(clipif->n_connections >= MAX_CLIPBOARDS){
            shutdown(new_client_fd,SHUT_RDWR);
            CLOSE(new_client_fd);
            SHOW_WARNING("Could not accept new connection: maximum number of connections reached.");
        }
        else if(( err = sendData(new_client_fd,&p_hello,sizeof(struct packet_sync)) ) <1){
            shutdown(new_client_fd,SHUT_RDWR);
            CLOSE(new_client_fd);
            SHOW_WARNING("Could not accept new connection: error sending syncronization packet: %lli.",err);
        }
        else{
            int *i = malloc(sizeof(int));
            for(*i = 0; *i < MAX_CLIPBOARDS; (*i)++){
                if(0 == pthread_mutex_trylock(&clipif->connections[*i].lock)){
                    if(clipif->connections[*i].sock_fd <= 0){
                        break;
                    }
                    else pthread_mutex_unlock(&clipif->connections[*i].lock);
                }
            }
            
            if(*i == MAX_CLIPBOARDS){
                SHOW_ERROR("Zombie connection detected, rejecting new connections.");
                CLOSE(new_client_fd);
                free(i);
                continue;
            }

            clipif->n_connections++;
            clipif->connections[*i].sock_fd = new_client_fd;
            pthread_mutex_unlock(&clipif->connections[*i].lock);

            SHOW_INFO("A clipboard connected");
            SHOW_INFO("Connected clipboards: %d/%d", clipif->n_connections,MAX_CLIPBOARDS);
            pthread_create(&clipif->connections[*i].recv_thread,NULL,clipif_slave,(void *) i);
        }
        pthread_mutex_unlock(&clipif->lock);
    }
    return NULL;
}

int clipif_init(int socket){
    long long err;
    int i;

    /*Init memmory*/
    mem_init();

    /*Init time syncronization structure*/
    if( (err = time_init() ) != 0){
        SHOW_ERROR("Could not connect to clipboard: error receiving syncronization packet: %lli",err);
        mem_finish();
    }

    /*Init Clipboards interface structure*/
    ASSERT_RETV(clipif == NULL,ERR_IF_EXISTS,"Distributed clipboard interface already initialized .");

    clipif = malloc(sizeof(distributed_interface));
    ASSERT_RETV(clipif != NULL,ERR_MEM_ALLOC,"Error allocating memory for distributed clipboard interface.");
    memset(clipif,0,sizeof(distributed_interface));

    /*Init atomic queue to forward packets created by slave threads to the broadcaster thread*/
    clipif->broadcasts = queue_create();

    pthread_mutex_init(&clipif->lock,NULL);

    /*Init one lock for each connection slot*/
    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(pthread_mutex_init(&clipif->connections[i].lock,NULL) != 0){
            SHOW_ERROR("Could not create lock for queue mutual exclusion.");
            queue_destroy(clipif->broadcasts);
            free(clipif);
            time_finish();
            mem_finish();
            return ERR_LOCK_CREATE;
        }
    }

    if(socket){
        /*Connect to root clipboard*/
        struct packet_sync hello_p;
        unsigned t1,t2;
        int * index;

        t1 = time_m_local();

        /*Expecting syncronization hello packet from root*/
        err = recvData(socket,&hello_p,sizeof(hello_p));

        if(err < (long long) sizeof(hello_p)){
            SHOW_ERROR("Could not connect to clipboard: error receiving syncronization packet: %lli",err);
            for(i = 0; i < MAX_CLIPBOARDS; i++){
                pthread_mutex_destroy(&clipif->connections[i].lock);
            }
            CLOSE(socket);
            mem_finish();
            time_finish();
            free(clipif);
            return ERR_CONNECT;
        }
        
        t2= time_m_local();

        /*Calculate delay to reach root, using it to syncronize clocks*/
        clipif->parentDelay = (t2 - t1)/2;
        time_m_sync(hello_p.time + clipif->parentDelay);

        index = malloc(sizeof(int));

        *index = 0;

        /*Add root to connected clipboards array*/
        clipif->connections[0].sock_fd = socket;
        clipif->parentSock = socket;
        clipif->n_connections++;

        SHOW_INFO("Connected clipboards: %d/%d", clipif->n_connections,MAX_CLIPBOARDS);

        /*Start listening to clipboard messages*/
        pthread_create(&clipif->connections[0].recv_thread,NULL,clipif_slave,(void *) index); 
    }
    else{
        /*Since the launch was done in single mode, the clipboard should consider 
          himself a root,thus sending periodic clock syncing messages to each child*/
        clipif->parentSock = -1;
        pthread_create(&clipif->clock_thread,NULL,clipif_clock_man,NULL);
    }

    /*Create thread to broadcast messages*/
    pthread_create(&clipif->master_thread,NULL,clipif_broadcast,NULL);
    return 0;
}

void clipif_finalize(){
    struct packet * bye_p;
    broadcast_t * b;
    int i,err;

    /*Send all clipboards a goodbye packet*/
    bye_p = malloc(sizeof(struct packet));
    b = malloc(sizeof(broadcast_t));

    bye_p->packetType = PACKET_GOODBYE;
    b->from = 0;
    b->p = bye_p;

    queue_push(clipif->broadcasts,b);
    
    /*Access to clipif->hasparent must be atomic*/
    pthread_mutex_lock(&clipif->lock);
    if(clipif->parentSock == -1){
        /*Running as root, need to finish clock sync thread*/
        if( (err = pthread_cancel(clipif->clock_thread)) != 0){
            SHOW_ERROR("Problem found while finishing clock thread: %d",err);
        }
        else if( (err = pthread_join(clipif->clock_thread,NULL) ) != 0){
            SHOW_ERROR("Problem found while finishing thread \"clock thread\": %d",err);
        }
        else SHOW_INFO("Clock thread finished.");
    }
    pthread_mutex_unlock(&clipif->lock);

    /*Stop broadcasting new messages*/
    queue_terminate(clipif->broadcasts);
    /*Stop broadcasting thread*/
    if( (err = pthread_join(clipif->master_thread,NULL) ) != 0){
        SHOW_ERROR("Problem found while finishing thread \"master_thread\": %d",err);
    }
    else SHOW_INFO("All remaining responses sent.");

    /*Close each slave communication thread*/
    for(i = 0; i < MAX_CLIPBOARDS; i++){
        pthread_mutex_lock(&clipif->connections[i].lock);

        if(clipif->connections[i].sock_fd > 0){
            shutdown(clipif->connections[i].sock_fd,SHUT_RDWR);
            pthread_mutex_unlock(&clipif->connections[i].lock);

            if( (err = pthread_cancel(clipif->connections[i].recv_thread)) != 0){
                SHOW_ERROR("Problem found while finishing thread %d: %d",i,err);
            }
            else if( (err = pthread_join(clipif->connections[i].recv_thread,NULL)) != 0){
                SHOW_ERROR("Error finishing thread \"clip_thread\": %d",err);
            }
            else SHOW_INFO("Slave thread %d finished",i);
        }
        else pthread_mutex_unlock(&clipif->connections[i].lock);
    }

    
    for(i = 0; i < MAX_CLIPBOARDS; i++){
        pthread_mutex_destroy(&clipif->connections[i].lock);
    }
    
    pthread_mutex_destroy(&clipif->lock);
    
    queue_destroy(clipif->broadcasts);
    time_finish();
    mem_finish();
    free(clipif);
}
