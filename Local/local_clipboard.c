
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "common.h"
#include "if.h"


#ifndef h_addr
#define h_addr h_addr_list[0]
#endif

pthread_mutex_t print_lock;

int main(int argc, char *argv[]){
    int err,i,myport,*temp;
    int apps_sock, clips_sock;
    struct sockaddr_in peer_addr, my_addr;
    struct sockaddr_un app_my_addr;

    struct hostent * hostinfo;

    pthread_t clip_thread, app_thread;
    int sock = 0, port = 0;
    char usage[200];

    ASSERT_RETV(pthread_mutex_init(&print_lock,NULL) == 0,ERR_MUTEX_CREATE,"Could not create lock for stdout mutual exclusion.");

    sprintf(usage,"Usage: %s -p <port> [-c <peer ip> <peer port>]",argv[0]);

    for(i = 0; i < argc - 1;i++){
        if(strncmp(argv[i],"-p",2) == 0){
            myport = atoi(argv[i+1]);
        }
    }
    if(myport <= 0){/*TODO usage*/
        SHOW_ERROR("No valid port specified.");
        SHOW_INFO("%s",usage);
        return ERR_NO_PORT;
    }

    if(argc > 1 && strncmp(argv[1],"-c",2) == 0){
        
        if(argc < 3){
            SHOW_ERROR("Can not run in connected mode: No peer IP address specified.");
            return ERR_NO_IP;
        }

        if((hostinfo= gethostbyname(argv[2])) == NULL) {
            SHOW_ERROR("%s unknown IP address.",argv[2]);
            return ERR_INVALID_IP;
        }

        if(argc < 4){
            SHOW_ERROR("Can not run in connected mode: No peer port specified.");
            return ERR_NO_PORT;
        }

        if( ( port = atoi(argv[3]) ) == 0){
            SHOW_ERROR("Can not run in connected mode: Invalid port.");
            return ERR_INV_PORT;         
        }
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            SHOW_ERROR("Connected Mode: Could not create socket.");
            return ERR_SOCKET;
        }

        memset(&peer_addr,0,sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_addr = *(struct in_addr *) hostinfo->h_addr;
        peer_addr.sin_port = htons(port);

        if (connect(sock, (struct sockaddr *) &peer_addr,sizeof(struct sockaddr)) == -1) {
            SHOW_ERROR("Could not connect to %s",argv[2]);
            return ERR_CANT_CONNECT;
        }
    }

    clips_sock = socket(AF_INET, SOCK_STREAM, 0);
    apps_sock = socket(AF_UNIX, SOCK_STREAM, 0);


    if (apps_sock  == -1 ||clips_sock == -1){
        SHOW_ERROR("Can not create socket for listening: %s",strerror(errno));
        return ERR_SOCKET;
    } 

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(myport);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    app_my_addr.sun_family = AF_UNIX;
    sprintf(app_my_addr.sun_path,"CLIPBOARD_SOCKET");

    err = bind(clips_sock,(struct sockaddr *) &my_addr,sizeof(struct sockaddr_in));
    if(err == -1){
        SHOW_ERROR("Can not bind net socket: %s",strerror(errno));
        CLOSE(clips_sock);
        if(sock) CLOSE(sock);
        return ERR_BIND_SOCKET;
    }

    err = bind(apps_sock,(struct sockaddr *) &app_my_addr,sizeof(struct sockaddr_un));
    if(err == -1){
        SHOW_ERROR("Can not bind unix socket: %s",strerror(errno));
        CLOSE(clips_sock);
        CLOSE(apps_sock);
        if(sock) CLOSE(sock);
        remove("CLIPBOARD_SOCKET");
        return ERR_BIND_SOCKET;
    }

    /*Ready Destributed Clipboard interface*/
    if ( (err = dclif_init(sock) ) != 0){
        return err;
    }
    /*Ready Applications interface*/
    if ( (err = appif_init() ) != 0){
        return err;
    }

    temp = malloc(sizeof(int));
    *temp = clips_sock;
    pthread_create(&clip_thread,NULL,dclif_listen,(void *)temp);

    temp = malloc(sizeof(int));
    *temp = apps_sock;
    pthread_create(&app_thread,NULL,appif_listen,(void *) temp);

    while(getchar() != 'q');

    if( (err = pthread_cancel(app_thread) ) != 0){
        SHOW_ERROR("Error canceling thread \"app_thread\": %d",err);
    }
    else if( (err = pthread_join(app_thread,NULL) ) != 0){
        SHOW_ERROR("Error finishing thread \"app_thread\": %d",err);
    }

    if( (err = pthread_cancel(clip_thread) ) != 0){
        SHOW_ERROR("Error canceling thread \"clip_thread\": %d",err);
    }
    else if( (err = pthread_join(clip_thread,NULL) ) != 0){
        SHOW_ERROR("Error finishing thread \"clip_thread\": %d",err);
    }

    appif_finalize();
    dclif_finalize();
    CLOSE(clips_sock);
    CLOSE(apps_sock);
    remove("CLIPBOARD_SOCKET");
    pthread_mutex_destroy(&print_lock);
    return 0;
}