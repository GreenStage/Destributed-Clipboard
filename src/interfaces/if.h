
#ifndef IF_HEADER
#define IF_HEADER

#define MAX_CLIPBOARDS 20
#define MAX_APPS 20

#define ERR_IF_EXISTS 1
#define ERR_LOCK_CREATE 2

#define CONN_TYPES 2

typedef enum{
    CONN_NONE = 0x0,
    CONN_CLIPBOARD,
    CONN_APPLICATION
} connection_type_t;

void time_m_sync(unsigned now);
unsigned time_m_now();
unsigned time_m_local();

int dclif_init(int socket);
void *dclif_listen(void * socket);
void dclif_add_broadcast(void *data,int from);
void dclif_finalize();

int appif_init();
void *appif_listen(void * socket);
void appif_finalize();

long long sendData(int sock,void * buf, int size);
long long recvData(int sock,void * buf,int size);

typedef enum packet_type_{
    PACKET_NONE = 0x0,
    PACKET_HELLO,
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
    uint8_t region;
    uint32_t dataSize;
    uint32_t recv_at;
    uint8_t data[0];
}__attribute__((__packed__));

struct packet_sync{
    uint8_t packetType;
    uint32_t time;
}__attribute__((__packed__));
#endif