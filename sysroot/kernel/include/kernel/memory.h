#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED

#include "stddef.h"

void memory_init();
void *kcalloc(int size);
void *kcallocco(int size, int alignment, int boundary);
void *kmalloc(int size);
void *dma_kmalloc(size_t size);
void *kmallocco(int size, int alignment, int boundary);
void *dma_kmallocco(size_t size, size_t alignment, size_t boundary);
void kfree(void *ptr);

#endif
