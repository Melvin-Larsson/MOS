#include "kernel/physpage.h"
#include "kernel/allocator.h"

#include "stdint.h"
#include "stdlib.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

typedef enum{
   Memory = 1,
   Reserved = 2,
}AddressRangeType;

typedef struct{
   uint64_t address;
   uint64_t length;
   uint32_t type;
}AddressRange;

static Allocator *pageAllocator;


void physpage_init(){
   volatile uint32_t *base = ((volatile uint32_t *)0x500);
   uint32_t length = *base / sizeof(AddressRange);

   AddressRange *addressRangeTable = (AddressRange*)(base + 1);

   pageAllocator = allocator_init(0, 0);

   for(uint32_t i = 0; i < length; i++){
      if(addressRangeTable[i].type != Memory){
         continue;
      }
      allocator_release(
            pageAllocator,
            (addressRangeTable[i].address + 4 * 1024 - 1) / (4 * 1024),
            addressRangeTable[i].length / (4 * 1024));
   }
}

uint64_t physpage_getPage4KB(){
   AllocatedArea area = allocator_get(pageAllocator, 1);
   if(area.size == 1){
      return area.address;
   }
   return 0;
}
uint64_t physpage_getPage4MB(){
   AllocatedArea area = allocator_get(pageAllocator, 1024);
   if(area.size == 1024){
      return area.address / 1024;
   }
   return 0;
}

uint64_t physpage_getPage4KBHigh(){
   AllocatedArea area = allocator_getHinted(pageAllocator, 1, AllocatorHintPreferHighAddresses);
   if(area.size == 1){
      return area.address;
   }
   return 0;
}
uint64_t physpage_getPage4MBHigh(){
   AllocatedArea area = allocator_getHinted(pageAllocator, 1024, AllocatorHintPreferHighAddresses);
   if(area.size == 1024){
      return area.address;
   }
   return 0;
}

void physpage_releasePage4KB(uint64_t page){
   allocator_release(pageAllocator, page, 1);
}
void physpage_releasePage4MB(uint64_t page){
   allocator_release(pageAllocator, page * 1024, 1024);
}

void physpage_markPagesAsUsed4MB(uint64_t page, uint32_t count){
   allocator_markAsReserved(pageAllocator, page * 1024, count * 1024);
}
void physpage_markPagesAsUsed4KB(uint64_t page, uint32_t count){
   allocator_markAsReserved(pageAllocator, page, count);
}
