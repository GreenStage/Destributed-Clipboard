#include <stdlib.h>
#include <string.h>

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

