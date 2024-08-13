#include "kernel/interrupt.h"
#include "kernel/registers.h"
#include "kernel/logging.h"
#include "kernel/kernel-io.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#define GDT_CODE_SEGMENT 0x08
#define IDT_MAX_DESCRIPTIONS 256

__attribute__((aligned(0x10)))
static InterruptDescriptor interruptDescriptorTable[256];
static InterruptTableDescriptor interruptTableDescriptor;

static void (*interruptHandlers[256])(ExceptionInfo, void *);
static void *interruptData[256];

extern void (*interrupt_addr_table[0x81])(void);


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
      loggError("%d: Interrupt %d, %X %X %X", interruptNr, interruptVector, info->errorCode, info->instructionOffset, info->codeSegment);
      while(1);
   }
}

typedef enum{
   setColor = 0,
   getColor,
   printf,
   clear
}StdioFunctions;

uint32_t stdio(uint16_t function, uint32_t param1, uint32_t param2){
   switch(function){
      case printf:
      {
         char *str = (char*)param1;
         va_list args = (va_list)param2;
         vkprintf(str, args);
         break;
      }
      case clear:
         kclear();
         break;
      case setColor:
      {
         KIOColor color = param1;
         kio_setColor(color);
         break;
      }
      case getColor:
      {
         kprintf("color %X\n", kio_getColor());
         return kio_getColor();
      }
   }
   return 0;
}

//Asume value is returned in eax
uint32_t syscall_handler(uint32_t eax, uint32_t param1, uint32_t param2){
   uint32_t functionClass = eax & 0xFFFF;
   uint32_t function = eax >> 16; 

   loggDebug("Syscall %d %d\n", functionClass, function);

   uint32_t result = 0;
   switch(functionClass){
      case 1:
         result =  stdio(function, param1, param2);
         break;
   }

   return result;
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

   memset((void*)interruptDescriptorTable, 0, interruptTableDescriptor.limit);
   for(uint8_t pos = 0; pos < 0x80; pos++){
      setInterruptDescriptor(pos, interrupt_addr_table[pos], 0x8E);
   }
   setInterruptDescriptor(0x80, interrupt_addr_table[0x80], 0xEE);


   dissablePIC();

   __asm__ volatile ("lidt %0" : : "m"(interruptTableDescriptor));
   loggDebug("Initialized interrupt table");
   __asm__ volatile ("sti");
   loggInfo("Interrupts activated");
}

InterruptStatus interrupt_setHandler(void (*interruptHandler)(ExceptionInfo, void *), void *data, uint8_t vector){
   if(interruptHandlers[vector]){
      loggError("Interrupt handler already defined");
      return InterruptStatusVectorAlreadyDefined;
   }

   interruptHandlers[vector] = interruptHandler;
   interruptData[vector] = data;

   return InterruptStatusSuccess;
}
