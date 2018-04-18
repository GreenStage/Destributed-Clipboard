#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "../common.h"
#include "../mem/clmem.h"
#include "../thread/queue.h"
#include "if.h"

typedef struct connection_{
        int sock_fd;
        pthread_t comm_thread;
} connection_t;

typedef struct application_interface_{
    int n_connections;
    connection_t connections[MAX_APPS];
    pthread_mutex_t lock;
    bool run;
} application_interface;

application_interface *appif = NULL;

int sendData(int sock,void * buf, int size){
    int sendBytes = 0;
    int ret;
    while(sendBytes < size){
        if( (ret = send(sock,buf+sendBytes,size-sendBytes,0)) == -1){
            return ret;
        }
        else sendBytes += ret;
    }
    return sendBytes;
}

void close_connection(void * i){
    int index = *((int*)i);
    free(i);

    pthread_mutex_lock(&appif->lock);
    appif->n_connections--;
    pthread_mutex_unlock(&appif->lock);

    SHOW_INFO("Closing socket %d.",appif->connections[index].sock_fd);
    CLOSE(appif->connections[index].sock_fd);
    appif->connections[index].sock_fd = 0;
}

void * appif_slave(void * index){
    int err = 1,index_,sock;
    unsigned sendSize,pBytes;
    unsigned long displacement;
    unsigned time_n;
    struct packet p;
    void * full_packet, *response;

    index_ = *((int*)index);
    sock = appif->connections[index_].sock_fd;
    

    displacement = (unsigned long) sizeof(struct packet);
    ASSERT_RETV(appif != NULL,NULL,"Applications interface not initiliazed.");
    pthread_cleanup_push(close_connection,index);
    while(appif->run) {
        if(err < 1) break;
        
        memset(&p,0,sizeof(p));
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if( (err = recvData(sock,&p,sizeof(p))) != sizeof(p)){
            continue;
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        full_packet = NULL;response = NULL;
        switch(p.packetType){
            case PACKET_REQUEST_PASTE:
                response = malloc(sizeof(struct packet) + p.dataSize);
                ((struct packet*)response)->packetType = PACKET_RESPONSE_PASTE;
                ((struct packet*)response)->recv_at = time_m_now();
                ((struct packet*)response)->dataSize = mem_get(p.region,(void*)(response + displacement),p.dataSize);
                SHOW_INFO("PASTE: %s",(char*) (((struct packet*)response)->data));
                sendSize = sizeof(struct packet) + ((struct packet*)response)->dataSize;
                break;

            case PACKET_REQUEST_WAIT:
                response = malloc(sizeof(struct packet) + p.dataSize);
                ((struct packet*)response)->packetType = PACKET_RESPONSE_NOTIFY;
                ((struct packet*)response)->recv_at = time_m_now();
                ((struct packet*)response)->dataSize = mem_wait(p.region,(void*)(response + displacement),p.dataSize);
                sendSize = sizeof(struct packet) + ((struct packet*)response)->dataSize;
                SHOW_INFO("NOTIFY: %s",(char*) (((struct packet*)response)->data));
                break;

            case PACKET_REQUEST_COPY:
                full_packet = malloc(sizeof(struct packet) + p.dataSize);
                if( (err = recvData(sock,full_packet + displacement,p.dataSize)) <1){
                    free(full_packet);
                    full_packet = NULL;
                    break;
                }

                time_n = time_m_now();
                pBytes = mem_put(p.region,(void*) (full_packet + displacement),p.dataSize,time_n);

                response = malloc(sizeof(struct packet));
                ((struct packet*)response)->packetType = PACKET_RESPONSE_ACK;
                ((struct packet*)response)->recv_at = time_n;
                ((struct packet*)response)->dataSize = pBytes;
                sendSize = sizeof(struct packet);

                if(pBytes == 0){
                    free(full_packet);
                }else{
                    memcpy(full_packet,&p,sizeof(p));
                    ((struct packet*)full_packet)->recv_at = time_n;
                    dclif_add_broadcast(full_packet,sock);
                }
                break;

            case PACKET_GOODBYE:
                err = 0;
                break;
            default: /*This should not happen, what's the deal with the application?*/
                SHOW_WARNING("Invalid packet.");
                break;
        }
        if(response){
            ((struct packet*)response)->region = p.region;
            SHOW_INFO("Sending response to application %d",index_);
            err = sendData(sock,response,sendSize);
            free(response);
        }
    }
    if(err < 1){

        if(err == 0) SHOW_INFO("Connection closed with application %d.",index_);
        else SHOW_WARNING("Error reading from socket with application %d: %s.",index_,strerror(errno));

        SHOW_INFO("Connected applications: %d/%d",appif->n_connections,MAX_APPS);
    }
    pthread_cleanup_pop(0);
    close_connection(index);
    return NULL;
}

void *appif_listen(void * socket){
    int sock;
    int new_client_fd;
    struct sockaddr_un new_client;
    unsigned addr_size;


    ASSERT_RETV(socket != NULL,NULL,"No socket passed.");
    sock =  *((int*)socket);
    free(socket);

    addr_size = sizeof(struct sockaddr_un);

    ASSERT_RETV(appif != NULL,NULL,"Applications interface not initiliazed.");
    ASSERT_RETV(sock > 0,NULL,"Invalid socket %d.",sock);
    ASSERT_RETV(listen(sock,MAX_APPS) == 0,NULL,"Can not listen with socket: %s.",strerror(errno));

    SHOW_INFO("Started to listen to applications connection requests.");
    while(1){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        new_client_fd = accept(sock,(struct sockaddr *) &new_client,&addr_size);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        

        pthread_mutex_lock(&appif->lock);
        if(new_client_fd < 0){
            SHOW_WARNING("Could not accept new connection: invalid socket.");
        }
        else if(appif->n_connections >= MAX_APPS){
            shutdown(new_client_fd,SHUT_RDWR);
            CLOSE(new_client_fd);
            SHOW_WARNING("Could not accept new connection: maximum number of connections reached.");
        }
        else{
            int *i = malloc(sizeof(int));
            for(*i = 0; *i < MAX_APPS; (*i)++){
                if(appif->connections[*i].sock_fd <= 0){
                    break;
                }
            }
            ASSERT_RETV(*i != MAX_APPS,NULL,"Zombie connection detected. exiting thread...");
            appif->n_connections++;
            appif->connections[*i].sock_fd = new_client_fd;

            SHOW_INFO("An application connected");
            SHOW_INFO("Connected applications: %d/%d",appif->n_connections,MAX_APPS);
            pthread_create(&appif->connections[*i].comm_thread,NULL,appif_slave,(void *) i);
        }
        pthread_mutex_unlock(&appif->lock);
    }
    return NULL;
}

int appif_init(){

    ASSERT_RETV(appif == NULL,ERR_IF_EXISTS,"No connection info passed.");

    appif = malloc(sizeof(application_interface));
    ASSERT_RETV(appif != NULL,ERR_MEM_ALLOC,"Error allocating memory for application interface.");

    memset(appif,0,sizeof(application_interface));
    pthread_mutex_init(&appif->lock,NULL);
    appif->run = true;

    return 0;
}

void appif_finalize(){
    int i,err;

    appif->run = false;

    for(i = 0; i < MAX_APPS; i++){
        if(appif->connections[i].sock_fd > 0){
            shutdown(appif->connections[i].sock_fd,SHUT_RDWR);
            if( (err = pthread_cancel(appif->connections[i].comm_thread)) != 0){
                SHOW_ERROR("Problem found while finishing communication thread %d: %d",i,err);
            }
            else if( (err = pthread_join(appif->connections[i].comm_thread,NULL)) != 0){
                SHOW_ERROR("Error finishing communication thread: %d",err);
            }
            else SHOW_INFO("Communication thread %d finished",i);
        }
    }

    pthread_mutex_destroy(&appif->lock);
    free(appif);
}
