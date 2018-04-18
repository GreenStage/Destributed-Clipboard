#include <stdlib.h>
#include <stdio.h>
#include "clipboard.h"
#include <string.h>
#include <unistd.h>

void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}
#define STR_LEN 2000
int main(int argc,char * agv[]){
    char command;
    char command_data[1000];
    int clip,region;
    int exit_ = 0;

    if(argc < 2) exit(1);

    if( (clip = clipboard_connect(argv[1]) )<1){
        printf("ups");
        return 1;
    }
    while(!exit_){
        if( 3 > scanf("%c %d %s",&command,&region,command_data)) continue;

        switch(command){
            case 'c':
                printf("COPY: %d\n",clipboard_copy(clip,region,command_data,strlen(command_data) + 1));
                break;
            case 'p':
                printf("PASTE: %d\n",clipboard_paste(clip,region,command_data,10000));
                printf("DATA: %s",command_data);
                break;
            case 'w':
                printf("WAIT: %d\n",clipboard_wait(clip,region,command_data,10000));
                printf("DATA: %s",command_data);
                break;     
            case 'q':
                exit_ = 1;   
            default:
                break;    
        }
    }
}