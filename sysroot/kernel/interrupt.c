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

typedef struct{
   void (*handle)(ExceptionInfo, void *);
   void *data;
}ExceptionHandler;

static ExceptionHandler exceptionHandlers[32];
static InterruptHandler interruptHandlers[256];

extern void (*interrupt_addr_table[0x81])(void);

static uint8_t getFreeInterruptVector(uint8_t count, bool aligned);

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
   ExceptionHandler handler = exceptionHandlers[interruptVector];

   if(handler.handle){
      loggDebug("%d: Interrupt vector: %d, error code: %X instruction offset: %X code segment: %X\n", interruptNr, interruptVector, info->errorCode, info->instructionOffset, info->codeSegment);
      handler.handle(*info, handler.data);
   }else{
      loggError("%d: Interrupt vector: %d, error code: %X instruction offset: %X code segment: %X\n", interruptNr, interruptVector, info->errorCode, info->instructionOffset, info->codeSegment);
      while(1);
   }
}

void interrupt_handler(unsigned char interruptVector){
   interruptNr++;
   InterruptHandler handler = interruptHandlers[interruptVector];
   if(handler.handle){
      loggDebug("%d: Interrupt vector %d (%s)", interruptNr, interruptVector, handler.name);
      handler.handle(handler.data);
   }else{
      loggError("%d: Interrupt vector %d", interruptNr, interruptVector);
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
      default:
         loggDebug("Unknown syscall");
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
   memset(exceptionHandlers, 0, sizeof(exceptionHandlers));

   memset((void*)interruptDescriptorTable, 0, interruptTableDescriptor.limit);
   for(uint8_t pos = 0; pos < 0x80; pos++){
      setInterruptDescriptor(pos, interrupt_addr_table[pos], 0x8E);
   }
   setInterruptDescriptor(0x80, interrupt_addr_table[0x80], 0xEE);

   dissablePIC();

   __asm__ volatile ("lidt %0" : : "m"(interruptTableDescriptor));
   __asm__ volatile ("sti");
}

InterruptStatus interrupt_setExceptionHandler(void (*handler)(ExceptionInfo, void *), void *data, uint8_t vector){
   if(vector >= 32){
      loggError("Invalid exception vector %d. Max is 31", vector);
      return InterruptStatusInvalidVector;
   }

   if(exceptionHandlers[vector].handle){
      loggError("Interrupt handler already defined");
      return InterruptStatusVectorAlreadyDefined;
   }

   exceptionHandlers[vector] = (ExceptionHandler){
      .handle = handler,
      .data = data,
   };
   
   return InterruptStatusSuccess;
}

uint8_t interrupt_setHandler(void (*handler)(void *), void *data, char *name){
   uint8_t vector = getFreeInterruptVector(1, false);
   if(vector == 0){
      return 0;
   }

   interruptHandlers[vector] = (InterruptHandler){
      .data = data,
      .handle = handler,
      .name = name
   };

   return vector;
}

uint8_t interrupt_setContinuousHandlers(InterruptHandler *handler, uint8_t handlerCount, bool aligned, char *name){
   uint8_t firstVector = getFreeInterruptVector(handlerCount, aligned);
   if(firstVector == 0){
      return 0;
   }

   for(int i = 0; i < handlerCount; i++){
      interruptHandlers[firstVector + i] = (InterruptHandler){
         .handle = handler[i].handle,
         .data = handler[i].data,
         .name = name,
      };
   }

   return firstVector;
}

InterruptStatus interrupt_setHardwareHandler(void (*interruptHandler)(void), uint8_t vector, InterruptPrivilegeLevel privilegeLevel){
   setInterruptDescriptor(vector, interruptHandler, 0x8E | privilegeLevel << 5);

   return InterruptStatusSuccess;
}

static bool areHandlersContiniuouslyFree(uint8_t startIndex, uint8_t count){
   if(startIndex + count >= 255){
      return false;
   }

   for(int i = startIndex; i < startIndex + count; i++){
      if(interruptHandlers[i].handle){
         return false;
      }
   }

   return true;
}

static uint8_t getFreeInterruptVector(uint8_t count, bool aligned){
   for(uint8_t i = 32; i < 255; i++){
      if(aligned && i % count != 0){
         continue;
      }

      if(areHandlersContiniuouslyFree(i, count)){
         return i;
      }

   }

   return 0;
}

