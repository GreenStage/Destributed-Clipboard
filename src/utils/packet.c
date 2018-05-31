
#include <sys/socket.h>
#include "packet.h"

long long sendData(int sock,void * buf, int size){
    int sendBytes = 0;
    while(sendBytes < size){
        int ret;
        if( (ret = send(sock,(char*)buf+sendBytes,size-sendBytes,0)) == -1){
            return ret;
        }
        else sendBytes += ret;
    }
    return sendBytes;
}

long long recvData(int sock,void * buf, int size){
    int recvBytes = 0;
    while(recvBytes < size){
        int ret;
        if((ret = recv(sock,buf,size,MSG_WAITALL)) < 1){
            return ret;
        }
        else recvBytes += ret;
    }
    return recvBytes;
}