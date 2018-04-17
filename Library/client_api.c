
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/un.h>
#include <sys/socket.h>

#include "clipboard.h"

#define SHOW_ERRORS
#define ERROR_MESSAGE_SIZE 400

#define MIN(A,B) (A) > (B) ? (B) : (A)

#ifdef SHOW_ERRORS
#define SHOW_ERROR(...) \
do {  \
    fprintf(stderr,__VA_ARGS__);\
} while(0)

#else
#define SHOW_ERROR(t,r,...) do{} while(0)
#endif

#define recvData(sock,buf,size) recv(sock,buf,size,MSG_WAITALL)

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

typedef enum packet_type_{
    PACKET_NONE = 0x0,
    PACKET_HELLO,

    PACKET_REQUEST_START = 0x80,
    PACKET_REQUEST_PASTE,
    PACKET_REQUEST_COPY,
    PACKET_REQUEST_WAIT,
    
    PACKET_RESPONSE_ACK = 0xC0,
    PACKET_RESPONSE_PASTE,
    PACKET_RESPONSE_NOTIFY,
} packet_type;

struct packet{
    uint8_t packetType;
    uint8_t region;
    uint16_t dataSize;
    uint32_t recv_at;
    uint8_t data[0];
}__attribute__((__packed__));

struct packet_sync{
    uint8_t packetType;
    uint32_t time;
}__attribute__((__packed__));


int clipboard_connect(char * clipboard_dir){
    
    int sock;
    struct sockaddr_un addr;
    
    sock = socket(AF_UNIX,SOCK_STREAM,0);

    if(sock == -1){
        SHOW_ERROR("Can not create socket: %s",strerror(errno));
        return ERROR_CONNECT_CLIPBOARD;
    }

    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path,"%s/CLIPBOARD_SOCKET",clipboard_dir);

    if(connect(sock,(struct sockaddr *) &addr,sizeof(struct sockaddr_un)) == -1){
        SHOW_ERROR("Can not connect to local clipboard: %s",strerror(errno));
        return ERROR_CONNECT_CLIPBOARD;
    }

    return sock;
}

int clipboard_copy(int clipboard_id, int region, void *buf, size_t count){
    int retval;
    struct packet * p;
    unsigned p_length = sizeof(struct packet) + count;

    if(region < 0 || region > 9){
        SHOW_ERROR("Invalid region: %s",strerror(errno));  
        return 0;
    }

    p = (struct packet*) malloc(p_length);

    p->packetType = PACKET_REQUEST_COPY;
    p->region = (uint8_t) region;
    p->recv_at = 0;
    p->dataSize = count;
    memcpy(p->data,buf,count);

    retval = sendData(clipboard_id,p,p_length); 

    if(retval == -1){
        SHOW_ERROR("Can not copy data to local clipboard: %s",strerror(errno));  
        free(p);  
        return 0;
    }

    if( ( retval = recvData(clipboard_id,(void*) p,sizeof(struct packet)) < 1) ){
        SHOW_ERROR("Can not copy data to local clipboard: %s",strerror(errno));  
        free(p);  
        return 0;
    }

    free(p);
    return count;
}



int clipboard_paste(int clipboard_id, int region, void *buf, size_t count){
    int retval;
    struct packet p;
    struct packet recv_p;

    if(region < 0 || region > 9){
        SHOW_ERROR("Invalid region: %s",strerror(errno));  
        return 0;
    }

    p.packetType = PACKET_REQUEST_PASTE;
    p.region = region;
    p.dataSize = count;

    if(send(clipboard_id,(const char*) &p,sizeof(struct packet),0) == -1){
        SHOW_ERROR("Can not request clipboard data: %s",strerror(errno));    
        return 0;
    }

    if( ( retval = recvData(clipboard_id,&recv_p,sizeof(struct packet)) ) < 1){
        SHOW_ERROR("Can not receive data from local clipboard: %s",strerror(errno));    
        return 0;
     }

    if(recv_p.packetType != PACKET_RESPONSE_PASTE){
        SHOW_ERROR("Invalid response type received from local clipboard: %d",recv_p.packetType);    
        return 0;        
    }

    if( ( retval = recvData(clipboard_id,&buf,recv_p.dataSize ) < 1) ){
        SHOW_ERROR("CanrecvData not copy data to local clipboard: %s",strerror(errno));    
        return 0;
    }

    /*Something went wrong with the clipboard, this should not happen*/
     if(recv_p.dataSize > count){
         uint8_t * trash = (uint8_t *) malloc(recv_p.dataSize - count);
         if( recvData(clipboard_id,trash,MIN(recv_p.dataSize,count) < 1) ){
            SHOW_ERROR("Error cleaning socket: %s",strerror(errno));
         }
         free(trash);
     }

     return retval;
}

int clipboard_wait(int clipboard_id, int region, void *buf, size_t count){
    int retval;
    struct packet p;
    struct packet recv_p;

    if(region < 0 || region > 9){
        SHOW_ERROR("Invalid region: %s",strerror(errno));  
        return 0;
    }

    p.packetType = PACKET_REQUEST_WAIT;
    p.region = region;
    p.dataSize = count;

    if(sendData(clipboard_id, &p,sizeof(struct packet)) == -1){
        SHOW_ERROR("Can not request clipboard data: %s",strerror(errno));    
        return 0;
    }

    if( ( retval = recvData(clipboard_id,&recv_p,sizeof(struct packet)) ) < 1){
        SHOW_ERROR("Can not receive data from local clipboard: %s",strerror(errno));    
        return 0;
     }

    if(recv_p.packetType != PACKET_RESPONSE_NOTIFY){
        SHOW_ERROR("Invalid response type received from local clipboard: %d",recv_p.packetType);    
        return 0;        
    }

    if( ( retval = recvData(clipboard_id,&buf,MIN(recv_p.dataSize,count) ) < 1) ){
        SHOW_ERROR("Can not copy data to local clipboard: %s",strerror(errno));    
        return 0;
     }

    /*Something went wrong with the clipboard, this should not happen*/
     if(recv_p.dataSize > count){
         uint8_t * trash = (uint8_t *) malloc(recv_p.dataSize - count);

         if( recvData(clipboard_id,&buf,recv_p.dataSize - count) < 1){
            SHOW_ERROR("Error cleaning socket: %s",strerror(errno));
         }
     }

     return retval;
}

void clipboard_close(int clipboard_id){
    close(clipboard_id);
}