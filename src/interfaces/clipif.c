

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include <unistd.h>
#include <netinet/in.h>
#ifdef __linux_
#include <linux/tcp.h>
#endif
#include <sys/socket.h>

#include "../common.h"
#include "../mem/clmem.h"
#include "../thread/queue.h"
#include "../utils/time.h"
#include "../utils/packet.h"
#include "if.h"

typedef struct connection_{
        int sock_fd;
        pthread_t recv_thread;
        pthread_mutex_t lock;
} connection_t;

typedef struct distributed_interface_{
    int n_connections;
    int parentSock, hasparent;
    unsigned parentDelay;
    connection_t connections[MAX_CLIPBOARDS];
    pthread_mutex_t lock;
    queue * broadcasts;
    pthread_t master_thread;
    pthread_t clock_thread;
} distributed_interface;

typedef struct broadcast_t_{
    struct packet * p;
    int from;
} broadcast_t;

distributed_interface *clipif = NULL;

void *clipif_clock_man(){
    struct packet_sync p;
    p.packetType = PACKET_TIME_SYNC;

    while(1){
        sleep(10);
        p.time = time_m_now();
        SHOW_WARNING("SYNC TIME %u",p.time);
        clipif_add_broadcast(&p,-1);
    }
}

int clipif_add_broadcast(void *data,int from){
    int err;
    broadcast_t *broadcast;

    ASSERT_RETV(clipif != NULL,1,"Distributed clipboards interface not initialized.");
    ASSERT_RETV(clipif->broadcasts != NULL,2,"Broadcast queue not initialized.");

    broadcast = malloc(sizeof(broadcast_t));
    ASSERT_RETV(broadcast != NULL,3,"Error allocating memory for broadcast structure.");

    broadcast->from = from;
    broadcast->p = (struct packet*) data;

    if((err = queue_push(clipif->broadcasts,(void*) broadcast)) != 0){
        free(broadcast);
        return err;
    }
    else return 0;
}

void clipif_close_conn(void * i){
    int index = *((int*)i);
    int socket;
    free(i);

    
    pthread_mutex_lock(&clipif->connections[index].lock);
    socket = clipif->connections[index].sock_fd;

    SHOW_INFO("Closing socket %d.",socket);
    CLOSE(socket);
    clipif->connections[index].sock_fd = 0;
    pthread_mutex_unlock(&clipif->connections[index].lock);
    
    /*Make sure that access to clif->hasparent and clipif->n_connections variables
    is atomic*/
    pthread_mutex_lock(&clipif->lock);
    if(clipif->parentSock == socket){
        clipif->hasparent = 0;
        clipif->parentDelay = 0;
        SHOW_INFO("Connection with master closed, becoming master...");
        pthread_create(&clipif->clock_thread,NULL,clipif_clock_man,NULL);
    }
    clipif->n_connections--;
    pthread_mutex_unlock(&clipif->lock);

}

void * clipif_slave(void * index){
    unsigned pBytes,pSize,pDataSize,pSyncSize;
    char buffer[sizeof(struct packet_data)], *dataBuffer;
    struct packet * p;

    long long err = 1;
    int sock,index_,err2;

    index_ = *((int*)index);
    sock = clipif->connections[index_].sock_fd;

    ASSERT_RETV(clipif != NULL,NULL,"Distributed clipboards interface not initialized.");
    pthread_cleanup_push(clipif_close_conn,index);

    pSize = sizeof(struct packet);
    pDataSize = sizeof(struct packet_data);
    pSyncSize = sizeof(struct packet_sync);

    p = (struct packet*) &buffer;

    while(1) {
        if(err < 1) break;
        struct packet_data * pd = NULL;
        struct packet_sync * ps = NULL;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if( (err = recvData(sock,p,pSize)) != pSize ){
            continue;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        switch(p->packetType){

            case PACKET_TIME_SYNC:
                /*Only should listen to syncronization packets from parent*/
                if(sock == clipif->parentSock){
                    dataBuffer = malloc(pSyncSize);
                    ps = (struct packet_sync*) dataBuffer;

                    err = recvData(sock,dataBuffer + pSize,pSyncSize - pSize);
                    if(err != pSyncSize - pSize){
                        SHOW_WARNING("Invalid packet size %lli.",err);
                        break;
                    }

                    SHOW_INFO("TIME SYNC PACKET %u",ps->time);
                    time_m_sync(ps->time + clipif->parentDelay);

                    ps->packetType = PACKET_TIME_SYNC;
                    ps->time = time_m_now();
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
    int i;
    broadcast_t * msg;
    uint32_t size;

    ASSERT_RETV(clipif != NULL,NULL,"Distributed clipboard interface not initialized.");

    while( ( msg = (broadcast_t*) queue_pop(clipif->broadcasts) ) != NULL) {

        for(i = 0; i < MAX_CLIPBOARDS;i++){
            pthread_mutex_lock(&clipif->connections[i].lock);

            if(clipif->connections[i].sock_fd > 0 && (clipif->connections[i].sock_fd != msg->from )) {
                SHOW_INFO("Broadcasting data from %d to clipboard %d",msg->from,i);
                switch(msg->p->packetType){
                    case PACKET_GOODBYE:
                        size = sizeof(struct packet);
                        break;
                    case PACKET_TIME_SYNC:
                        SHOW_INFO("SEND SYNC PACKET %u",((struct packet_sync*)msg->p)->time);
                        size = sizeof(struct packet_sync);
                        break;
                    case PACKET_REQUEST_COPY:
                        size = sizeof(struct packet_data) + ((struct packet_data*)msg->p)->dataSize;
                        break;
                    default:
                        break;
                }
                sendData(clipif->connections[i].sock_fd,(void*) msg->p,size);
            }
            pthread_mutex_unlock(&clipif->connections[i].lock);
        }
        if(msg->from != -1)
            free(msg->p);
        free(msg);
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
            
            /*TODO ERROR HANDLING*/
            ASSERT_RETV(*i != MAX_CLIPBOARDS,NULL,"Zombie connection detected. exiting thread...");
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
    struct packet_sync hello_p;
    long long err;
    int i;

    /*Init memmory*/
    mem_init();

    /*Init time syncronization*/
    if( (err = time_init() ) != 0){
        SHOW_ERROR("Could not connect to clipboard: error receiving syncronization packet: %lli",err);
        mem_finish();
    }

    ASSERT_RETV(clipif == NULL,ERR_IF_EXISTS,"Distributed clipboard interface already initialized .");

    clipif = malloc(sizeof(distributed_interface));
    ASSERT_RETV(clipif != NULL,ERR_MEM_ALLOC,"Error allocating memory for distributed clipboard interface.");

    memset(clipif,0,sizeof(distributed_interface));
    clipif->broadcasts = queue_create();
    clipif->parentSock = 0;

    pthread_mutex_init(&clipif->lock,NULL);
    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(pthread_mutex_init(&clipif->connections[i].lock,NULL) != 0){
            SHOW_ERROR("Could not create lock for queue mutual exclusion.");
            clipif->connections[i].sock_fd = 0;
            continue;;
        }
    }

    if(socket){
        unsigned t1,t2;
        int * index;

        t1 = time_m_local();
        err = recvData(socket,&hello_p,sizeof(hello_p));

        if(err < (int)sizeof(hello_p)){
            SHOW_ERROR("Could not connect to clipboard: error receiving syncronization packet: %lli",err);
            CLOSE(socket);
            mem_finish();
            time_finish();
            free(clipif);
            for(i = 0; i < MAX_CLIPBOARDS; i++){
                pthread_mutex_destroy(&clipif->connections[i].lock);
            }
            return ERR_CONNECT;
        }
        
        t2= time_m_local();

        clipif->parentDelay = (t2 - t1)/2;
        time_m_sync(hello_p.time + clipif->parentDelay);

        index = malloc(sizeof(int));

        *index = 0;
        clipif->connections[0].sock_fd = socket;
        clipif->parentSock = socket;
        clipif->n_connections++;

        SHOW_INFO("Connected clipboards: %d/%d", clipif->n_connections,MAX_CLIPBOARDS);
        clipif->hasparent = 1;
        pthread_create(&clipif->connections[0].recv_thread,NULL,clipif_slave,(void *) index); 
    }
    else{
        pthread_create(&clipif->clock_thread,NULL,clipif_clock_man,NULL);
    }

    pthread_create(&clipif->master_thread,NULL,clipif_broadcast,NULL);
    return 0;
}

void clipif_finalize(){
    int i,err;
    struct packet * bye_p;
    broadcast_t * b;

    bye_p = malloc(sizeof(struct packet));
    b = malloc(sizeof(broadcast_t));

    bye_p->packetType = PACKET_GOODBYE;
    b->from = 0;
    b->p = bye_p;

    queue_push(clipif->broadcasts,b);
    queue_terminate(clipif->broadcasts);
    
    pthread_mutex_lock(&clipif->lock);
    if(!clipif->hasparent){
        if( (err = pthread_cancel(clipif->clock_thread)) != 0){
            SHOW_ERROR("Problem found while finishing clock thread: %d",err);
        }
        else if( (err = pthread_join(clipif->clock_thread,NULL) ) != 0){
            SHOW_ERROR("Problem found while finishing thread \"clock thread\": %d",err);
        }
        else SHOW_INFO("Clock thread finished.");
    }
    pthread_mutex_unlock(&clipif->lock);

    if( (err = pthread_join(clipif->master_thread,NULL) ) != 0){
        SHOW_ERROR("Problem found while finishing thread \"master_thread\": %d",err);
    }
    else SHOW_INFO("All remaining responses sent.");

    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(clipif->connections[i].sock_fd > 0){
            shutdown(clipif->connections[i].sock_fd,SHUT_RDWR);
            if( (err = pthread_cancel(clipif->connections[i].recv_thread)) != 0){
                SHOW_ERROR("Problem found while finishing thread %d: %d",i,err);
            }
            else if( (err = pthread_join(clipif->connections[i].recv_thread,NULL)) != 0){
                SHOW_ERROR("Error finishing thread \"clip_thread\": %d",err);
            }
            else SHOW_INFO("Slave thread %d finished",i);
        }
    }

    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(clipif->connections[i].sock_fd > 0){
            SHOW_INFO("Closing socket %d.",clipif->connections[i].sock_fd);
            shutdown(clipif->connections[i].sock_fd,SHUT_RDWR);
            CLOSE(clipif->connections[i].sock_fd);
        }
        pthread_mutex_destroy(&clipif->connections[i].lock);
    }
    pthread_mutex_destroy(&clipif->lock);

    time_finish();
    mem_finish();
    free(clipif);
}
