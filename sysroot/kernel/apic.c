#include "kernel/apic.h"
#include "stdio.h"
#include "stdint.h"

#define CPUID_APIC_BIT (1<<9)

int apic_isPresent(){
    uint32_t edx = 0;
   __asm__ volatile ("\
         mov $0x1, %%eax  \n\
         cpuid         \n\
         mov %%eax, %0" : "=r"(edx));
        
   return edx & CPUID_APIC_BIT ? 1 : 0;
}
