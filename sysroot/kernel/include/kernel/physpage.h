#ifndef PHYSPAGE_H_INCLUDED
#define PHYSPAGE_H_INCLUDED

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "memory-allocator.h"

bool physpage_init(Memory *memory);
uint64_t physpage_getPage4KB();
uint64_t physpage_getPage4MB();

uint64_t physpage_getPage4KBHigh();
uint64_t physpage_getPage4MBHigh();

void physpage_releasePage4KB(uint64_t page);
void physpage_releasePage4MB(uint64_t page);

void physpage_markPagesAsUsed4MB(uint64_t page, uint32_t count);
void physpage_markPagesAsUsed4KB(uint64_t page, uint32_t count);

#endif
