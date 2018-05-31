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
#include "../utils/time.h"
#include "../utils/packet.h"
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

void appif_close_conn(void * i){
    int index = *((int*)i);

    SHOW_INFO("Cleaning up communication thread with sock %d",appif->connections[index].sock_fd);
    free(i);

    pthread_mutex_lock(&appif->lock);
    appif->n_connections--;
   
    SHOW_INFO("Closing socket %d.",appif->connections[index].sock_fd);
    CLOSE(appif->connections[index].sock_fd);
    appif->connections[index].sock_fd = 0;
    pthread_mutex_unlock(&appif->lock);
}

void * appif_slave(void * index){
    unsigned pBytes,sendSize,pSize,pDataSize,pFetchSize;
    char buffer[sizeof(struct packet_data)], *dataBuffer;
    struct packet_fetch * pf;
    struct packet_data * pd;
    struct packet * p, *response;

    int index_,sock,err2;
    unsigned time_n;
    long long err = 1;

    index_ = *((int*)index);
    sock = appif->connections[index_].sock_fd;

    ASSERT_RETV(appif != NULL,NULL,"Applications interface not initialized.");
    pthread_cleanup_push(appif_close_conn,index);

    pSize = sizeof(struct packet);
    pFetchSize = sizeof(struct packet_fetch);
    pDataSize = sizeof(struct packet_data);

    p = (struct packet*) &buffer;
    while(1) {
        if(err < 1) break;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if( (err = recvData(sock,p,pSize)) != pSize){
            continue;
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        pd = NULL; pf = NULL; response = NULL;
        
        switch(p->packetType){
            case PACKET_REQUEST_PASTE:
                err = recvData(sock,p + pSize,pFetchSize - pSize);
                if(err != pFetchSize - pSize){
                    break;
                }

                pf = (struct packet_fetch*) p;

                dataBuffer = malloc(pDataSize + pf->dataSize);
                pd = (struct packet_data*) dataBuffer;

                pd->packetType = PACKET_RESPONSE_PASTE;
                pd->region = pf->region;
                pd->dataSize = mem_get(pf->region,(void*)(dataBuffer + pDataSize),pf->dataSize);
                pd->recv_at = time_m_now();

                SHOW_INFO("PASTE %u from region %d: %s",pd->dataSize,pd->region,(char*) (dataBuffer + pDataSize));
                sendSize = pDataSize + pd->dataSize;
                response = (struct packet*) pd;
                break;

            case PACKET_REQUEST_WAIT:
                err = recvData(sock,p + pSize,pFetchSize - pSize);
                if(err != pFetchSize - pSize){
                    break;
                }
                
                pf = (struct packet_fetch*) p;

                dataBuffer = malloc(pDataSize + pf->dataSize);
                pd = (struct packet_data*) dataBuffer;

                pd->packetType = PACKET_RESPONSE_NOTIFY;
                pd->recv_at = time_m_now();
                pd->region = pf->region;
                pd->dataSize = mem_wait(pf->region,(void*)(dataBuffer + pDataSize),pf->dataSize);

                SHOW_INFO("NOTIFY: %s",(char*) (pd->data));
                sendSize = pDataSize + pd->dataSize;
                response = (struct packet*) pd;
                break;

            case PACKET_REQUEST_COPY:
                err = recvData(sock,p + pSize,pDataSize- pSize);
                if(err != pDataSize - pSize){
                    break;
                }

                pBytes = ((struct packet_data*) p)->dataSize;

                dataBuffer = malloc(pDataSize + pBytes);
                memcpy(dataBuffer,p,pDataSize);

                if( (err = recvData(sock,(dataBuffer + pDataSize),pBytes)) != pBytes){
                    SHOW_WARNING("Invalid data size received: %lli, expected %u",err,pBytes);
                    free(pd);
                    break;
                }

                pd = (struct packet_data*) dataBuffer;
                
                time_n = time_m_now();
                printf("mem put %s\n",(char*)pd->data);
                pBytes = mem_put(pd->region,(void*) (dataBuffer + pDataSize),pd->dataSize,time_n);

                pf = malloc(sizeof(struct packet_fetch));
                pf->packetType = PACKET_RESPONSE_ACK;
                pf->region = pd->region;
                pf->dataSize = pBytes;
                
                response = (struct packet*) pf;
                sendSize = pFetchSize;

                if(pBytes == 0){
                    free(pd);
                }else{
                    pd->recv_at = time_n;
                    if( (err2 = clipif_add_broadcast(pd,sock)) != 0){
                        SHOW_ERROR("Could not broadcast message from application %d: %d",sock,err2);
                        free(pd);
                    }
                }
                break;

            case PACKET_GOODBYE:
                err = 0;
                break;
            default: /*This should not happen, what's the deal with the application?*/
                SHOW_WARNING("Invalid packet %u.",p->packetType);
                break;
        }
        if(response){
            SHOW_INFO("Sending response to application %d",index_);
            err = sendData(sock,response,sendSize);
            free(response);
        }
    }
    if(err < 1){
        if(err == 0) SHOW_INFO("Connection closed with application %d.",index_);
        else SHOW_WARNING("Error reading from socket with application %d: %s.",index_,strerror(errno));
        
        pthread_mutex_lock(&appif->lock);
        SHOW_INFO("Connected applications: %d/%d",appif->n_connections,MAX_APPS);
        pthread_mutex_unlock(&appif->lock);
    }
    pthread_cleanup_pop(0);
    appif_close_conn(index);
    return NULL;
}

void *appif_listen(void * socket){
    struct sockaddr_un new_client;
    unsigned addr_size;
    int sock;
    
    ASSERT_RETV(socket != NULL,NULL,"No socket passed.");
    sock =  *((int*)socket);
    free(socket);

    addr_size = sizeof(struct sockaddr_un);

    ASSERT_RETV(appif != NULL,NULL,"Applications interface not initialized.");
    ASSERT_RETV(sock > 0,NULL,"Invalid socket %d.",sock);
    ASSERT_RETV(listen(sock,MAX_APPS) == 0,NULL,"Can not listen with socket: %s.",strerror(errno));

    SHOW_INFO("Started to listen to applications connection requests.");
    while(1){
        int new_client_fd;
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

    ASSERT_RETV(appif == NULL,ERR_IF_EXISTS,"Application interface already initialized.");

    appif = malloc(sizeof(application_interface));
    ASSERT_RETV(appif != NULL,ERR_MEM_ALLOC,"Error allocating memory for application interface.");

    memset(appif,0,sizeof(application_interface));
    pthread_mutex_init(&appif->lock,NULL);

    return 0;
}

void appif_finalize(){
    int i,err;
    
    for(i = 0; i < MAX_APPS; i++){
        if(appif->connections[i].sock_fd > 0){
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
