/*----------------------------------------*
 * Random String Copy/Paste Speed tests   *
 *----------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h> 

#include "../../Library/clipboard.h"

#define NTESTS 10

/*Defaults to 100MB*/
#define STR_LEN 33

/*Random string generator, implemented as in:
    https://stackoverflow.com/questions/15767691/
*/
void rand_str(char *dest, unsigned length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (--length > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *(dest++) = charset[index];
    }
    *dest = '\0';
}

int compare_str(char * s1,char * s2,size_t length){
    if(s1 == NULL || s2 == NULL){
        return -1;
    }
    else if(s1[length -1] != '\0'){
        return -2;
    }
    else if(s2[length -1] != '\0'){
        return -3;
    }
    else return abs(strcmp(s1,s2));
    
}

int main(int argc,char * argv[]){
    char * data;
    char  * rcvData;
    int res;
    int clip,region;
    int i = 0;
    time_t t;
    
    if(argc < 2) exit(1);

    if( (clip = clipboard_connect(argv[1]) )<1){
        fprintf(stderr,"Could not connect to clipboard");
        return 1;
    }
    data = malloc(STR_LEN);
    if(data == NULL){
        printf("Could not allocate memmory for buffer.\n");
        exit(-1);
    }

    rcvData = malloc(STR_LEN);
    if(rcvData == NULL){
        printf("Could not allocate memmory for buffer.\n");
        exit(-2);
    }

    printf("\nThis test copies and pastes random strings in a loop,\nchecking if the from paste is the same as the data sent to copy.\n");

    printf("\nStarting..\n");
    srand((unsigned) time(&t));

    for(i = 0; i < NTESTS; i++){
        rand_str(data, STR_LEN);
        region = rand()%10;
        
#ifdef DEBUG
        printf("Sending %d:\n", clipboard_copy(clip,region,data,STR_LEN));
        printf("%s\n",data);
        printf("Recv %d :\n",clipboard_paste(clip,region,rcvData,STR_LEN));
        
        printf("%s\n",rcvData);
#else
        clipboard_copy(clip,region,data,STR_LEN);
        clipboard_paste(clip,region,rcvData,STR_LEN);
#endif
        res = compare_str(rcvData,data,STR_LEN);
        printf("Test %d (region %d): ",i,region);
        if(res == 0){
            printf("OK.\n");
        }
        else if(res == -1){
            printf("Error - null string\n");
            break; 
        }
        else if(res == -2){
            printf("Error - String 1 is corrupted\n");
            break;
        }
        else if(res == -3){
            printf("Error - String 2 is corrupted\n");
            break;
        }
        else {
            printf("Error- String mismatch %d\n",res);
            break;
        }
        
    }
    clipboard_close(clip);
    free(data);
    free(rcvData);
    // printf("WAIT: %d\n",clipboard_wait(clip,region,command_data,10000));
}