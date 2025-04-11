#ifndef ALLOCATOR_H_INCLUDED
#define ALLOCATOR_H_INCLUDED

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "memory-allocator.h"

typedef enum{
   AllocatorHintPreferLowAddresses,
   AllocatorHintPreferHighAddresses,
}AllocatorHint;

typedef struct{
   uintptr_t address;
   uintptr_t size;
}AllocatedArea;

typedef struct{
   void *data;
}Allocator;

Allocator *allocator_initManaged(Memory *memory, uintptr_t address, size_t size);
Allocator* allocator_init(uintptr_t address, unsigned int size);
void allocator_free(Allocator *allocator);


/**
 * Allocates some interval of size 'size'
 *
 * Note that allocator_get will never run out of memory while performing this operation.
 */
AllocatedArea allocator_get(Allocator *allocator, int size);

/**
 * Allocates some interval of size 'size' using an AllocatorHint.
 *
 * Note that allocator_getHinted will never run out of memory while performing this operation.
 */
AllocatedArea allocator_getHinted(Allocator *allocator, int size, AllocatorHint hint);


/**
 * Releases some previously alloacted memory. 
 *
 * If allocator_release is unable to allocate memory for its internal structures it will return false.
 * Otheriwse true.
 */
bool allocator_release(Allocator *allocator, uintptr_t address, int size);

/**
 * Reserves some memory. 
 *
 * If allocator_markAsReserved is unable to allocate memory for its internal structures it will return false.
 * Otheriwse true.
 */
bool allocator_markAsReserved(Allocator *allocator, uintptr_t address, int size);

#endif
