#include "kernel/interrupt.h"
#include "kernel/registers.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#define GDT_CODE_SEGMENT 0x08
#define IDT_MAX_DESCRIPTIONS 256
__attribute__((aligned(0x10)))
static InterruptDescriptor interruptDescriptorTable[256];
static InterruptTableDescriptor interruptTableDescriptor;

extern void (*interrupt_addr_table[32])(void);


static void setInterruptDescriptor(uint8_t pos, void (*func)(void), uint8_t flags){
   InterruptDescriptor *desc = &interruptDescriptorTable[pos];

   desc->isrLow = (uint32_t)func & 0xFFFF;
   desc->isrHigh = (uint32_t)func >> 16;
   desc->segment = GDT_CODE_SEGMENT; 
   desc->attributes = flags;
   desc->reserved = 0;
}
static int interruptNr = 0;
void exception_handler(unsigned char interruptVector, ExceptionInfo *info){
   interruptNr++;
   printf("%d: Interrupt %d, %d %d %d\n", interruptNr, interruptVector, info->errorCode, info->instructionOffset, info->codeSegment);
   while(1);
}

/*static void dissablePIC(){
   __asm__ volatile ("mov 0xff, %%al");
   __asm__ volatile ("out %%al, 0xa1");
   __asm__ volatile ("out %%al, 0x21");
}*/
void interruptDescriptorTableInit(){
   interruptNr = 0;
   interruptTableDescriptor.base = (uint32_t)&interruptDescriptorTable[0];
   interruptTableDescriptor.limit = (uint16_t)sizeof(InterruptDescriptor) * IDT_MAX_DESCRIPTIONS - 1;

   memset(interruptDescriptorTable, 0, interruptTableDescriptor.limit);
   for(uint8_t pos = 0; pos < 32; pos++){
      setInterruptDescriptor(pos, interrupt_addr_table[pos], 0x8E);
   }
   __asm__ volatile ("lidt %0" : : "m"(interruptTableDescriptor));
   printf("Initialized interrupt table\n");
   printf("Activating interrupts\n");
   __asm__ volatile ("sti");
   printf("Interrupts activated\n");
}
