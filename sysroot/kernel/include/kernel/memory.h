#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED

void memory_init();
void *kcalloc(int size);
void *kcallocco(int size, int alignment, int boundary);
void *kmalloc(int size);
void *kmallocco(int size, int alignment, int boundary);
void kfree(void *ptr);

#endif
