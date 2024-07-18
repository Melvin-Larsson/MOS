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

static void (*interruptHandlers[256])(ExceptionInfo, void *);
static void *interruptData[256];

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
   if(interruptHandlers[interruptVector]){
      interruptHandlers[interruptVector](*info, interruptData[interruptVector]);
   }else{
      printf("%d: Interrupt %d, %d %d %d\n", interruptNr, interruptVector, info->errorCode, info->instructionOffset, info->codeSegment);
      while(1);
   }
}

static void dissablePIC(){
   uint8_t mask = 0xFF;
   __asm__ volatile ("out %[mask], $0xa1" : : [mask]"a"(mask));
   __asm__ volatile ("out %[mask], $0x21" : : [mask]"a"(mask));
}
void interruptDescriptorTableInit(){
   interruptNr = 0;
   interruptTableDescriptor.base = (uint32_t)&interruptDescriptorTable[0];
   interruptTableDescriptor.limit = (uint16_t)sizeof(InterruptDescriptor) * IDT_MAX_DESCRIPTIONS - 1;
   memset(interruptHandlers, 0, sizeof(interruptHandlers));
   memset(interruptData, 0, sizeof(interruptData));

   memset(interruptDescriptorTable, 0, interruptTableDescriptor.limit);
   for(uint8_t pos = 0; pos < 34; pos++){
      setInterruptDescriptor(pos, interrupt_addr_table[pos], 0x8E);
   }

   dissablePIC();

   __asm__ volatile ("lidt %0" : : "m"(interruptTableDescriptor));
   printf("Initialized interrupt table\n");
   printf("Activating interrupts\n");
   __asm__ volatile ("sti");
   printf("Interrupts activated\n");
}

InterruptStatus interrupt_setHandler(void (*interruptHandler)(ExceptionInfo, void *), void *data, uint8_t vector){
   if(interruptHandlers[vector]){
      printf("Interrupt handler already defined\n");
      return InterruptStatusVectorAlreadyDefined;
   }

   interruptHandlers[vector] = interruptHandler;
   interruptData[vector] = data;

   return InterruptStatusSuccess;
}
