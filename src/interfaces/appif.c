/*---------------------------------------------
    appif.c
    -Manages communication with applications.
----------------------------------------------*/

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


/*
  Single connection structure.
  Since we dont have a master thread accessing different connections 
  like in clipif.c, it is not worth having a lock for each connection.
*/
typedef struct connection_{
        int sock_fd;
        pthread_t comm_thread;
} connection_t;

/*Appications Interface Structure*/
typedef struct application_interface_{
    int n_connections;
    connection_t connections[MAX_APPS];
    pthread_cond_t slaveFinished;
    pthread_mutex_t lock;
} application_interface;

application_interface *appif = NULL;

/*-------------------------------
        Private functions
 -------------------------------*/
 
/*Clean up function called after exiting a slave thread*/
void appif_close_conn(void * i){
    int index = *((int*)i);
    int sock_;
    free(i);
    
    /*Access to the interface structure shall be atomic*/
    pthread_mutex_lock(&appif->lock);
    
    /*Detach pthread to ensure all its resources are freed when finished*/
	pthread_detach(appif->connections[index].comm_thread);
	
    appif->n_connections--;
    sock_ = appif->connections[index].sock_fd;
    
    SHOW_INFO("Closing socket %d.",sock_);
    SHOW_INFO("Connected applications: %d/%d",appif->n_connections,MAX_APPS);

    CLOSE(sock_);
    appif->connections[index].sock_fd = 0;
    pthread_mutex_unlock(&appif->lock);
    
    SHOW_INFO("Communication thread %d finished",index);
    pthread_cond_signal(&appif->slaveFinished);
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

    p = (struct packet*) buffer;
    while(1) {
        if(err < 1) break;

        /*Only allow cancellation to occur when waiting for messages,
          thus avoiding memory leaks*/
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if( (err = recvData(sock,buffer,pSize)) != pSize){
            continue;
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        pd = NULL; pf = NULL; response = NULL;
        
        switch(p->packetType){
            case PACKET_REQUEST_PASTE:
                /*Received a paste request*/

                /*Read fetch packet info*/
                err = recvData(sock,buffer + pSize,pFetchSize - pSize);
                if(err != pFetchSize - pSize){
                    break;
                }

                pf = (struct packet_fetch*) buffer;

                /*Prepare response buffer*/
                dataBuffer = malloc(pDataSize + pf->dataSize);
                if(dataBuffer == NULL){
                    SHOW_ERROR("Could not allocate memory for response paste packet size %u",pDataSize + pf->dataSize);
                    break;
                }

                pd = (struct packet_data*) dataBuffer;

                pd->packetType = PACKET_RESPONSE_PASTE;
                pd->region = pf->region;
                pd->recv_at = time_m_now();

                /*Fetch data from memory*/
                pd->dataSize = mem_get(pf->region,(void*)(dataBuffer + pDataSize),pf->dataSize);

                SHOW_INFO("PASTE %u from region %d",pd->dataSize,pd->region);
                sendSize = pDataSize + pd->dataSize;
                response = (struct packet*) pd;
                break;

            case PACKET_REQUEST_WAIT:
                /*Received a wait request*/

                /*Read fetch packet info*/
                err = recvData(sock,buffer + pSize,pFetchSize - pSize);
                if(err != pFetchSize - pSize){
                    break;
                }
                
                pf = (struct packet_fetch*) buffer;

                /*Prepare response buffer*/
                dataBuffer = malloc(pDataSize + pf->dataSize);
                if(dataBuffer == NULL){
                    SHOW_ERROR("Could not allocate memory for response wait packet size %u",pDataSize + pf->dataSize);
                    break;
                }

                pd = (struct packet_data*) dataBuffer;

                pd->packetType = PACKET_RESPONSE_NOTIFY;
                pd->recv_at = time_m_now();
                pd->region = pf->region;

                /*Wait for a data update from memory (blocking operation)*/
                pd->dataSize = mem_wait(pf->region,(void*)(dataBuffer + pDataSize),pf->dataSize);

                if(pd->dataSize == 0){
                    err = 0;
                }

                SHOW_INFO("NOTIFY: %u",pd->dataSize);
                sendSize = pDataSize + pd->dataSize;

                response = (struct packet*) pd;
                break;

            case PACKET_REQUEST_COPY:
                /*Received a copy request*/
                SHOW_WARNING("OIOIOI");
                /*Read data packet info*/
                err = recvData(sock,buffer + pSize,pDataSize- pSize);
                if(err != pDataSize - pSize){
                    break;
                }

                pBytes = ((struct packet_data*) buffer)->dataSize;

                /*Prepare broadcast packet*/
                dataBuffer = malloc(pDataSize + pBytes);
                if(dataBuffer == NULL){
                    SHOW_ERROR("Could not allocate memory for copy packet size %u",pDataSize + pBytes);
                    break;
                }
                memcpy(dataBuffer,buffer,pDataSize);

                /*Receive data to copy from*/
                if( (err = recvData(sock,(dataBuffer + pDataSize),pBytes)) != pBytes){
                    SHOW_WARNING("Invalid data size received: %lli, expected %u",err,pBytes);
                    free(pd);
                    break;
                }

                pd = (struct packet_data*) dataBuffer;
                
                /*Mark data entrance in the network with a time stamp*/
                time_n = time_m_now();

                /*Attempt to place it in memory*/
                pBytes = mem_put(pd->region,(void*) (dataBuffer + pDataSize),pd->dataSize,time_n);
                
                /*Prepare response*/
                pf = malloc(sizeof(struct packet_fetch));
                if(pf == NULL){
                    SHOW_ERROR("Could not allocate memory for response copy packet size %lu",sizeof(struct packet_fetch));
                }
                else{
                    response = (struct packet*) pf;
                    pf->packetType = PACKET_RESPONSE_ACK;
                    pf->region = pd->region;
                    pf->dataSize = pBytes;
                    sendSize = pFetchSize;
                }

                if(pBytes == 0){
                    /*Failed to place data in memory , dispose broadcast packet*/
                    free(pd);
                }else{
                    /*Success, forward packet to children*/
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
        }
        if(response){
            SHOW_INFO("Sending response to application %d",index_);
            err = MIN(sendData(sock,response,sendSize),err);
            free(response);
        }
    }
    if(err < 1){ /*Connection closed (0) or error (-1)*/
        if(err == 0) SHOW_INFO("Connection closed with application %d.",index_);
        else SHOW_WARNING("Error reading from socket with application %d: %s.",index_,strerror(errno));
    }

    /*Clean up*/
    pthread_cleanup_pop(0);
    appif_close_conn(index);
    return NULL;
}

/*-------------------------------
        Public functions
 -------------------------------*/

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

        /*Only accept cancellation requests on the blocking accept call,
          avoiding possible memory leaks*/
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        new_client_fd = accept(sock,(struct sockaddr *) &new_client,&addr_size);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        /*New connection, lock access to interface structure*/
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
            /*Find a free slot for this new connection*/
            for(*i = 0; *i < MAX_APPS; (*i)++){
                if(appif->connections[*i].sock_fd <= 0){
                    break;
                }
            }

            /*This wont happen, the connection counter is less than the max number
             of connections, but there is no free slot?*/
            ASSERT_RETV(*i != MAX_APPS,NULL,"Zombie connection detected. exiting thread...");

            appif->n_connections++;
            appif->connections[*i].sock_fd = new_client_fd;

            SHOW_INFO("An application connected");
            SHOW_INFO("Connected applications: %d/%d",appif->n_connections,MAX_APPS);

            /*Starting to read from connected application*/
            pthread_create(&appif->connections[*i].comm_thread,NULL,appif_slave,(void *) i);
        }
        pthread_mutex_unlock(&appif->lock);
    }
    return NULL;
}

int appif_init(){

    ASSERT_RETV(appif == NULL,-2,"Application interface already initialized.");

    /*Init interface structure*/
    appif = malloc(sizeof(application_interface));
    ASSERT_RETV(appif != NULL,-1,"Error allocating memory for application interface.");

    memset(appif,0,sizeof(application_interface));

    /*Init lock for atomic access to the interface structure*/
	if(pthread_mutex_init(&appif->lock,NULL) != 0){
        SHOW_ERROR("Could not create lock for interface structure.");
		free(appif);
        return -1;
    }
    
    /*Init conditional signal for finishing slaves*/
	if(pthread_cond_init(&appif->slaveFinished,NULL) != 0){
        SHOW_ERROR("Could not create condition trigger for slaves termination.");
		free(appif);
        return -1;
    }
    return 0;
}

void appif_finalize(){
    int i,err;

    /*Terminate alive slave threads*/
    for(i = 0; i < MAX_APPS; i++){
        if(appif->connections[i].sock_fd > 0){
            /*Sending cancellation request*/
            if( (err = pthread_cancel(appif->connections[i].comm_thread)) != 0){
                SHOW_ERROR("Problem found while finishing communication thread %d: %d",i,err);
            }
        }
    }

    /*Wait for all slaves to finish, note that pthread_join can not be use here
     * as all slaves threads need to be detached in order to avoid memory leaks
     * when finishing in run-time*/
    pthread_mutex_lock(&appif->lock);
    while(appif->n_connections > 0){
        pthread_cond_wait(&appif->slaveFinished,&appif->lock);
    }
    pthread_mutex_unlock(&appif->lock);

    if( (err = pthread_cond_destroy(&appif->slaveFinished)) ){
		SHOW_ERROR("Error finishing condtion for slaves termination: %d",err);
	}

    if( (err = pthread_mutex_destroy(&appif->lock)) != 0){
		SHOW_ERROR("Error destroying struct mutex %d",err);
	}
    free(appif);
}
