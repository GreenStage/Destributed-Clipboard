

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "common.h"
#include "if.h"
#include "clmem.h"
#include "utils/time_m.h"
#include "utils/queue.h"


typedef struct connection_{
        int sock_fd;
        pthread_t recv_thread;
        pthread_mutex_t lock;
} connection_t;

typedef struct distributed_interface_{
    int n_connections;
    connection_t connections[MAX_CLIPBOARDS];
    pthread_mutex_t lock;
    queue * broadcasts;
    bool run;
    pthread_t master_thread;
} distributed_interface;

typedef struct broadcast_t_{
    struct packet * p;
    int from;
} broadcast_t;

distributed_interface *dclif = NULL;


void dclif_add_broadcast(void *data,int from){
    broadcast_t *broadcast;

    ASSERT_RET(dclif != NULL,"Distributed clipboards interface not initiliazed.");
    ASSERT_RET(dclif->broadcasts != NULL,"Broadcast queue not initiliazed.");

    broadcast = malloc(sizeof(broadcast_t));
    ASSERT_RET(broadcast != NULL,"Error allocating memory for broadcast structure.");

    broadcast->from = from;
    broadcast->p = (struct packet*) data;
    queue_push(dclif->broadcasts,(void*) broadcast);
}

void * dclif_slave(void * index){
    int err = 1,sock,index_;
    unsigned long displacement;
    unsigned updated_tick;
    struct packet p;
    void * full_packet;

    index_ = *((int*)index);
    sock = dclif->connections[index_].sock_fd;
    free(index);

    displacement = (unsigned long) sizeof(struct packet);
    ASSERT_RETV(dclif != NULL,NULL,"Distributed clipboards interface not initiliazed.");

    while(dclif->run) {
        if(err < 1) break;
        
        memset(&p,0,sizeof(p));
        full_packet = NULL;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if( (err = recvData(sock,&p,sizeof(p)) != sizeof(p)) ){
            continue;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        if(p.packetType == PACKET_GOODBYE){
            err = 0;
            break;
        }

        full_packet = malloc(sizeof(struct packet) + p.dataSize);

        err = recvData(sock,full_packet + displacement,p.dataSize);

        if(err != p.dataSize || p.packetType != PACKET_REQUEST_COPY){
            SHOW_WARNING("Invalid packet %d.",p.packetType);
            free(full_packet);
        }
        else{
            updated_tick = mem_put(p.region,(void*) (full_packet + displacement),p.dataSize,p.recv_at);
            if(updated_tick == 0){
                free(full_packet);
            }
            else{
                memcpy(full_packet,&p,sizeof(p));
                dclif_add_broadcast(full_packet,sock);
            }
        }
    }
    if(err < 1){

        if(err == 0) SHOW_INFO("Connection closed with clipboard %d.",index_);
        else SHOW_WARNING("Error reading from socket with clipboard %d: %s.",index_,strerror(errno));
        
        pthread_mutex_lock(&dclif->connections[index_].lock);

        CLOSE(dclif->connections[index_].sock_fd);
        dclif->connections[index_].sock_fd = 0;

        pthread_mutex_lock(&dclif->lock);
        dclif->n_connections--;
        pthread_mutex_unlock(&dclif->lock);

        pthread_mutex_unlock(&dclif->connections[index_].lock);

        SHOW_INFO("Connected clipboards: %d/%d", dclif->n_connections,MAX_CLIPBOARDS);
    }
    return NULL;
}

void * dclif_broadcast(){
    int i;
    broadcast_t * msg;

    ASSERT_RETV(dclif != NULL,NULL,"Distributed clipboard interface not initiliazed.");

    while( ( msg = (broadcast_t*) queue_pop(dclif->broadcasts) ) != NULL) {

        for(i = 0; i < MAX_CLIPBOARDS;i++){
            pthread_mutex_lock(&dclif->connections[i].lock);

            if(dclif->connections[i].sock_fd > 0 && (dclif->connections[i].sock_fd != msg->from )) {
                SHOW_INFO("Broadcasting data from %d to clipboard %d",msg->from,i);
                sendData(dclif->connections[i].sock_fd,(void*) msg->p,sizeof(struct packet) + ((struct packet*)msg->p)->dataSize);
            }

            pthread_mutex_unlock(&dclif->connections[i].lock);
        }
        free(msg->p);
        free(msg);
    }
    return NULL;
}

void *dclif_listen(void * socket){
    int sock,err;
    struct packet_sync p_hello;
    int new_client_fd;
    struct sockaddr_in new_client;
    unsigned addr_size;

    ASSERT_RETV(socket != NULL,NULL,"No socket passed.");
    sock =  *((int*)socket);
    free(socket);

    addr_size = sizeof(struct sockaddr_in);

    p_hello.packetType = PACKET_HELLO;

    ASSERT_RETV(dclif != NULL,NULL,"Distributed Clipboard Interface not initiliazed.");
    ASSERT_RETV(sock > 0,NULL,"Invalid socket %d.",sock);
    ASSERT_RETV(listen(sock,MAX_CLIPBOARDS) == 0,NULL,"Can not listen on socket: %s.",strerror(errno));

    SHOW_INFO("Started to listen to clipboards connection requests.");
    while(1){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        new_client_fd = accept(sock, (struct sockaddr *) &new_client,&addr_size);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        p_hello.time = time_m_now();

        pthread_mutex_lock(&dclif->lock);
        if(new_client_fd < 0){
            SHOW_WARNING("Could not accept new connection: invalid socket.");
        }
        else if(dclif->n_connections >= MAX_CLIPBOARDS){
            shutdown(new_client_fd,SHUT_RDWR);
            CLOSE(new_client_fd);
            SHOW_WARNING("Could not accept new connection: maximum number of connections reached.");
        }
        else if(( err = sendData(new_client_fd,&p_hello,sizeof(struct packet_sync)) ) <1){
            shutdown(new_client_fd,SHUT_RDWR);
            CLOSE(new_client_fd);
            SHOW_WARNING("Could not accept new connection: error sending syncronization packet: %d.",err);
        }
        else{
            int *i = malloc(sizeof(int));
            for(*i = 0; *i < MAX_CLIPBOARDS; (*i)++){
                if(0 == pthread_mutex_trylock(&dclif->connections[*i].lock) &&
                    dclif->connections[*i].sock_fd <= 0){
                    break;
                }
            }
            /*TODO ERROR HANDLING*/
            ASSERT_RETV(*i != MAX_CLIPBOARDS,NULL,"Zombie connection detected. exiting thread...");
            dclif->n_connections++;
            dclif->connections[*i].sock_fd = new_client_fd;
            pthread_mutex_unlock(&dclif->connections[*i].lock);

            SHOW_INFO("A clipboard connected");
            SHOW_INFO("Connected clipboards: %d/%d", dclif->n_connections,MAX_CLIPBOARDS);
            pthread_create(&dclif->connections[*i].recv_thread,NULL,dclif_slave,(void *) i);
        }
        pthread_mutex_unlock(&dclif->lock);
    }
    return NULL;
}

int dclif_init(int socket){
    struct packet_sync hello_p;
    int i;
    /*Init memmory*/
    mem_init();

    /*Init time syncronization*/
    time_m_sync(0);

    ASSERT_RETV(dclif == NULL,ERR_IF_EXISTS,"Distributed clipboard interface already initiliazed .");

    dclif = malloc(sizeof(distributed_interface));
    ASSERT_RETV(dclif != NULL,ERR_MEM_ALLOC,"Error allocating memory for distributed clipboard interface.");

    memset(dclif,0,sizeof(distributed_interface));
    dclif->broadcasts = queue_create();
    dclif->run = true;

    pthread_mutex_init(&dclif->lock,NULL);
    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(pthread_mutex_init(&dclif->connections[i].lock,NULL) != 0){
            SHOW_ERROR("Could not create lock for queue mutual exclusion.");
            dclif->connections[i].sock_fd = 0;
            continue;;
        }
    }

    if(socket){
        int err;
        int * index;

        err = recvData(socket,&hello_p,sizeof(hello_p));

        if(err < (int)sizeof(hello_p)){
            SHOW_ERROR("Could not connect to clipboard: error receiving syncronization packet: %d",err);
            CLOSE(socket);
        }
        time_m_sync(hello_p.time);

        index = malloc(sizeof(int));

        *index = 0;
        dclif->connections[0].sock_fd = socket;
        dclif->n_connections++;

        SHOW_INFO("Connected clipboards: %d/%d", dclif->n_connections,MAX_CLIPBOARDS);
        pthread_create(&dclif->connections[0].recv_thread,NULL,dclif_slave,(void *) index); 
    }

    pthread_create(&dclif->master_thread,NULL,dclif_broadcast,NULL);
    return 0;
}

void dclif_finalize(){
    int i,err;
    struct packet * bye_p;
    broadcast_t * b;

    bye_p = malloc(sizeof(struct packet));
    b = malloc(sizeof(broadcast_t));

    bye_p->packetType = PACKET_GOODBYE;
    b->from = 0;
    b->p = bye_p;

    dclif->run = false;

    queue_push(dclif->broadcasts,b);
    queue_terminate(dclif->broadcasts);

    if( (err = pthread_join(dclif->master_thread,NULL) ) != 0){
        SHOW_ERROR("Problem found while finishing thread \"master_thread\": %d",err);
    }
    else SHOW_INFO("All remaining responses sent.");

    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(dclif->connections[i].sock_fd > 0){
            if( (err = pthread_cancel(dclif->connections[i].recv_thread)) != 0){
                SHOW_ERROR("Problem found while finishing thread %d: %d",i,err);
            }
            else if( (err = pthread_join(dclif->connections[i].recv_thread,NULL)) != 0){
                SHOW_ERROR("Error finishing thread \"clip_thread\": %d",err);
            }
            else SHOW_INFO("Slave thread %d finished",i);
        }
    }

    for(i = 0; i < MAX_CLIPBOARDS; i++){
        if(dclif->connections[i].sock_fd > 0){
            SHOW_INFO("Closing socket %d.",dclif->connections[i].sock_fd);
            shutdown(dclif->connections[i].sock_fd,SHUT_RDWR);
            CLOSE(dclif->connections[i].sock_fd);
        }
        pthread_mutex_destroy(&dclif->connections[i].lock);
    }
    pthread_mutex_destroy(&dclif->lock);
    mem_finish();
    free(dclif);
}
