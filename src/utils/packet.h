#ifndef PACKET_HEADER
#define PACKET_HEADER
#include <stdint.h>

/*Packet identifiers*/
typedef enum packet_type_{
    PACKET_NONE = 0x0,
    PACKET_TIME_SYNC,
    PACKET_GOODBYE,

    PACKET_REQUEST_START = 0x80,
    PACKET_REQUEST_PASTE,
    PACKET_REQUEST_COPY,
    PACKET_REQUEST_WAIT,
    
    PACKET_RESPONSE_ACK = 0xC0,
    PACKET_RESPONSE_PASTE,
    PACKET_RESPONSE_NOTIFY,
} packet_type;

/*Basic packet type*/
struct packet{
    uint8_t packetType;
}__attribute__((__packed__));

/*Packet that transports data*/
struct packet_data{
    uint8_t packetType;
    uint8_t region;
    uint32_t dataSize;
    uint32_t recv_at;
    uint8_t data[0];
}__attribute__((__packed__));

/*Packet that informs or requests data*/
struct packet_fetch{
    uint8_t packetType;
    uint8_t region;
    uint32_t dataSize;
}__attribute__((__packed__));

/*Syncronization packet*/
struct packet_sync{
    uint8_t packetType;
    uint32_t time;
}__attribute__((__packed__));

/*Reads data from "sock", blocking until "size" bytes are read, or 
*the socket becomes invalid*/
long long recvData(int sock,void * buf, int size);

/*Writes data to "sock", blocking until "size" bytes are sent, or 
*the socket becomes invalid*/
long long sendData(int sock,void * buf, int size);

#endif
