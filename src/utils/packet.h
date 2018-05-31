#ifndef PACKET_HEADER
#define PACKET_HEADER
#include <stdint.h>

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

struct packet{
    uint8_t packetType;
}__attribute__((__packed__));

struct packet_data{
    uint8_t packetType;
    uint8_t region;
    uint32_t dataSize;
    uint32_t recv_at;
    uint8_t data[0];
}__attribute__((__packed__));

struct packet_fetch{
    uint8_t packetType;
    uint8_t region;
    uint32_t dataSize;
}__attribute__((__packed__));

struct packet_sync{
    uint8_t packetType;
    uint32_t time;
}__attribute__((__packed__));


long long recvData(int sock,void * buf, int size);
long long sendData(int sock,void * buf, int size);

#endif
