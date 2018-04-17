#include <stdlib.h>
#include <stdio.h>
#include "clipboard.h"
#include <string.h>
#include <unistd.h>
#include <time.h>

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
int main(){
    char max_buff[255];
    char command[50];
    char data[STR_LEN + 1];
    int clip;
    int a = 0;
    int i, s;

    srand(time(NULL));
    sprintf(data,"testetestestestesteste");
    if( (clip = clipboard_connect("../Local") )<1){
        printf("ups");
        return 1;
    }
    while(1){
        s = rand() % STR_LEN;
        rand_str(data,s);
        usleep(200000);
        printf("Sending %s\n",data);
        printf("Copy returned: %d\n",clipboard_copy(clip,a++ %10,data,strlen(data) +1));

    }
}