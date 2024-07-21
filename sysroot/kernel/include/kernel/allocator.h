#ifndef ALLOCATOR_H_INCLUDED
#define ALLOCATOR_H_INCLUDED

#include "stdint.h"

typedef struct{
   uintptr_t address;
   uintptr_t size;
}AllocatedArea;

typedef struct{
   void *data;
}Allocator;

Allocator* allocator_init(uintptr_t address, unsigned int size);
void allocator_free(Allocator *allocator);

AllocatedArea allocator_get(Allocator *allocator, int size);
void allocator_release(Allocator *allocator, uintptr_t address, int size);

void allocator_markAsReserved(Allocator *allocator, uintptr_t address, int size);

#endif
