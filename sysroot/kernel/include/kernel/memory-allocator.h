#ifndef MEMORY_ALLOCATOR_H_INCLUDED
#define MEMORY_ALLOCATOR_H_INCLUDED

#include "stdbool.h"

typedef void* Memory;

Memory *memory_new(void *start, unsigned int length);
bool memory_append(Memory *memory, void *start, unsigned int size);
void *memory_malloc(Memory *memory, unsigned int size);
/* void *memory_calloc(Memory *memory, unsigned size); */
void memory_free(Memory *memory, void *ptr);
void *memory_mallocco(Memory *memory, unsigned size, unsigned alignment, unsigned boundary);

#endif
