#ifndef DESCRIPTORS_H_INCLUDED
#define DESCRIPTORS_H_INCLUDED

#include "stdint.h"

typedef enum{
   GdtOk,
   GdtFull
}GdtStatus;

typedef struct{
   uintptr_t address;

   uint32_t size : 20;
   int use4KBGranularity;

   int isCode;
   int expandDown;
   int writeEnable;
   int accessed;

   uint64_t descriptorPrivilegeLevel : 2;

   uint64_t is64BitCodeSegment : 1;
   uint64_t is32BitSegment : 1;
}GdtCodeDataDescriptor;

typedef struct{
   uintptr_t address;

   uint32_t size : 20;
   int use4KBGranularity;

   uint64_t descriptorPrivilegeLevel : 2;
}GdtTssDescriptor;

typedef struct{
   uintptr_t address;

   uint32_t size : 20;
   int use4KBGranularity;

   uint64_t descriptorPrivilegeLevel : 2;
}LdtDescriptor;

void gdt_init();

int gdt_addCodeDataDescriptor(GdtCodeDataDescriptor descriptor);
int gdt_addLdtDescriptor(LdtDescriptor descriptor);
int gdt_addTss32Descriptor(GdtTssDescriptor descriptor);

uint16_t gdt_getSize();
uintptr_t gdt_getAddress();

#endif
