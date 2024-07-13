#ifndef PHYSPAGE_H_INCLUDED
#define PHYSPAGE_H_INCLUDED

#include "stdint.h"

void physpage_init();
uint64_t physpage_getPage4KB();
uint64_t physpage_getPage4MB();
void physpage_releasePage4KB(uint64_t page);
void physpage_releasePage4MB(uint64_t page);

void physpage_markPagesAsUsed4MB(uint64_t page, uint32_t count);
void physpage_markPagesAsUsed4KB(uint64_t page, uint32_t count);

#endif
