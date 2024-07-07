#include "kernel/descriptors.h"
#include "stdlib.h"

#define GDT_ENTRIES 1024

typedef volatile struct{
   uint64_t segmentLimitLow : 16;
   uint64_t baseAddressLow : 24;
   uint64_t segmentType : 4;
   uint64_t descriptorType : 1;
   uint64_t descriptorPrivilegeLevel : 2;
   uint64_t segmentPresent : 1;
   uint64_t segmentLimitHigh : 4;
   uint64_t avl : 1;
   uint64_t is64BitCodeSegment : 1;
   uint64_t is32BitSegment : 1;
   uint64_t granularity : 1;
   uint64_t baseAddressHigh : 8;
}__attribute__((packed))SegmentDescriptor;

static GdtStatus addSegmentDescriptor(SegmentDescriptor descriptor);
static uint8_t getSegmentType(int isCode, int expandDown, int writeEnable, int accessed);
static void gdt_loadGdtRegister(uintptr_t address, uint16_t limit);

static SegmentDescriptor gdt[GDT_ENTRIES];

void gdt_init(){
   memset(gdt, 0, sizeof(gdt));
   gdt[1] = (SegmentDescriptor){
      .segmentLimitLow = 0xFFFF,
      .segmentType = getSegmentType(1, 0, 1, 0),
      .descriptorType = 1,
      .segmentPresent = 1,
      .segmentLimitHigh = 0xF,
      .is32BitSegment = 1,
      .granularity = 1,
   };
   gdt[2] = (SegmentDescriptor){
      .segmentLimitLow = 0xFFFF,
      .segmentType = getSegmentType(1, 0, 1, 0),
      .descriptorType = 1,
      .segmentPresent = 1,
      .segmentLimitHigh = 0xF,
      .is32BitSegment = 1,
      .granularity = 1,
   };
   gdt_loadGdtRegister((uintptr_t)gdt, sizeof(gdt) - 1);
}

int gdt_addCodeData32Descriptor(GdtCodeDataDescriptor descriptor){
   SegmentDescriptor segmentDescriptor = {
      .segmentLimitLow = descriptor.size & 0xFFFF,
      .baseAddressLow = descriptor.address & 0xFFFFFF,
      .segmentType = getSegmentType(
            descriptor.isCode,
            descriptor.expandDown,
            descriptor.writeEnable,
            descriptor.accessed),
      .descriptorType = 1,
      .descriptorPrivilegeLevel = descriptor.descriptorPrivilegeLevel,
      .segmentPresent = 1,
      .segmentLimitHigh = (descriptor.size >> 16) & 0xF,
      .is64BitCodeSegment = descriptor.is64BitCodeSegment,
      .is32BitSegment = descriptor.is32BitSegment,
      .granularity = descriptor.use4KBGranularity,
      .baseAddressHigh = (descriptor.address >> 24) & 0xFF
   };
   return addSegmentDescriptor(segmentDescriptor);
}

int gdt_addLdtDescriptor(LdtDescriptor descriptor){
   SegmentDescriptor segmentDescriptor = {
      .segmentLimitLow = descriptor.size & 0xFFFF,
      .baseAddressLow = descriptor.address & 0xFFFFFF,
      .segmentType = 0b0010,
      .descriptorType = 0,
      .descriptorPrivilegeLevel = descriptor.descriptorPrivilegeLevel,
      .segmentPresent = 1,
      .segmentLimitHigh = (descriptor.size >> 16) & 0xF,
      .granularity = descriptor.use4KBGranularity,
      .baseAddressHigh = (descriptor.address >> 24) & 0xFF
   };
   return addSegmentDescriptor(segmentDescriptor);
}

#include "stdio.h"

int gdt_addTss32Descriptor(GdtTssDescriptor descriptor){
   SegmentDescriptor segmentDescriptor = {
      .segmentLimitLow = descriptor.size & 0xFFFF,
      .baseAddressLow = descriptor.address & 0xFFFFFF,
      .segmentType = 0b1001,
      .descriptorType = 0,
      .descriptorPrivilegeLevel = descriptor.descriptorPrivilegeLevel,
      .segmentPresent = 1,
      .segmentLimitHigh = (descriptor.size >> 16) & 0xF,
      .granularity = descriptor.use4KBGranularity,
      .baseAddressHigh = (descriptor.address >> 24) & 0xFF
   };
   uint32_t *ptr = &segmentDescriptor;
   printf("12 %X %X\n", ptr[0], ptr[1]);
   return addSegmentDescriptor(segmentDescriptor);
}

static GdtStatus addSegmentDescriptor(SegmentDescriptor descriptor){
   for(int i = 1; i < GDT_ENTRIES; i++){
      if(!gdt[i].segmentPresent){
         gdt[i] = descriptor;
         return i;
      }
   }
   return 0;
}

uint16_t gdt_getSize(){
   uint8_t __attribute__((aligned(4)))temp[10];
   __asm__ volatile("sgdt %[addr]"
         : "=m"(temp)
         : [addr]"m"(temp)
         :
         );
   
   return *((uint16_t*)temp) + 1;
}

uintptr_t gdt_getAddress(){
   uint8_t temp[10];
   __asm__ volatile("sgdt %[addr]"
         : "=m"(temp)
         : [addr]"m"(temp)
         :
         );
   
   return *((uintptr_t*)&temp[2]);
}

static void gdt_loadGdtRegister(uintptr_t address, uint16_t limit){
   uint8_t __attribute__((aligned(4)))temp[10];
   memset(temp, 0, 10);
   memcpy(&temp[2], &address, sizeof(uintptr_t));
   memcpy(temp, &limit, 2);

   __asm__ volatile("lgdt %[addr]"
         : 
         : [addr]"m"(temp)
         :
         );
}

static uint8_t getSegmentType(int isCode, int expandDown, int writeEnable, int accessed){
   return (isCode != 0) << 3 | (expandDown != 0) << 3 | (writeEnable != 0) << 3 | (accessed != 0);
}
