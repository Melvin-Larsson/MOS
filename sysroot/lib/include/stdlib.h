#ifndef INCLUDE_STDLIB_H
#define INCLUDE_STDLIB_H

void stdlib_init();
void *memset(void *start, int val, int size);
void *memcpy(void* dst, const void *src, int size);
void *malloc(int size);
void *calloc(int size);
void *mallocco(int size, int alignment, int boundaries);
void *callocco(int size, int alignment, int boundaries);

void free(void *ptr);

#endif
