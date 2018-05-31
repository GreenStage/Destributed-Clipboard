#include <stdlib.h>
#include <stdio.h>
#include "../Library/clipboard.h"
#include <string.h>
#include <unistd.h>
#define STR_LEN 2000

int main(int argc,char * argv[]){
    char command;
    char command_data[1000];
    int clip,region;
    int exit_ = 0;
    int cc;
    if(argc < 2) exit(1);

    if( (clip = clipboard_connect(argv[1]) )<1){
        fprintf(stderr,"Could not connect to clipboard");
        return 1;
    }
    printf("This is an interactive test, usage:\n");
    printf("<command> [<region> [<data>]]:\n");
    printf("command:\n");
    printf("\tc: Copy\n");
    printf("\tp: Paste\n");
    printf("\tw: Wait\n");
    printf("\tq: Quit\n");
    while(!exit_){
        cc = scanf("%c",&command);
        if( 1 > cc) continue;

        switch(command){
            case 'c':
                cc = scanf("%d %[^\n]",&region,command_data);
                if(cc < 2)
                    printf("Missing values in expression.\n");
                else 
                    printf("COPY: %d\n",clipboard_copy(clip,region,command_data,strlen(command_data) + 1));
                break;
            case 'p':
                cc = scanf("%d%*[^\n]",&region);
                if(cc < 1)
                    printf("Missing values in expression.\n");
                else{
                    printf("PASTE: %d\n",clipboard_paste(clip,region,command_data,10000));
                    printf("DATA: %s\n",command_data);
                }

                break;
            case 'w':
                cc = scanf("%d%*[^\n]",&region);
                if(cc < 1)
                    printf("Missing values in expression.\n");
                else{
                    printf("WAIT: %d\n",clipboard_wait(clip,region,command_data,10000));
                    printf("DATA: %s\n",command_data);
                }
                break;     
            case 'q':
                exit_ = 1;   
            default:
                break;    
        }
    }
}