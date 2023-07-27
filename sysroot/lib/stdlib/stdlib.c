#include "stdlib.h"
void *memset(void *start, int val, int size){
    unsigned char* p = (unsigned char *)start;
    while(size > 0){
        *p++ = (unsigned char)val;
        size--;
    }
    return start;
}
