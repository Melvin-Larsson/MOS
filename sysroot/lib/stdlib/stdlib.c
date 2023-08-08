#include "stdlib.h"
void *memset(void *start, int val, int size){
    unsigned char* p = (unsigned char *)start;
    while(size > 0){
        *p++ = (unsigned char)val;
        size--;
    }
    return start;
}
void *memcpy(void* dst, const void *src, int size){
    unsigned char *dsti = (unsigned char*)dst;
    unsigned char *srci = (unsigned char*)src;
    while(size > 0){
        *dsti++ = *srci++;
        size--;
    }
    return (void*)dst;
}
