
#ifndef INTERRUPT_H_INCLUDED
#define INTERRUPT_H_INCLUDED

#include "stdint.h"
#include "stdbool.h"

typedef enum{
    InterruptStatusVectorAlreadyDefined,
    InterruptStatusSuccess,
    InterruptStatusInvalidVector,
}InterruptStatus;

typedef volatile struct{
    uint16_t isrLow;
    uint16_t segment;
    uint8_t reserved;
    uint8_t attributes;
    uint16_t isrHigh;
} __attribute__((packed)) InterruptDescriptor;

typedef struct{
    uint16_t limit;
    uint32_t base;
}__attribute__((packed)) InterruptTableDescriptor;

typedef struct{
    uint32_t codeSegment;
    uint32_t instructionOffset;
    union{
        uint32_t errorCode;
        struct{
            char EXT : 1;
            char IDT : 1;
            char TI : 1;
            short segmentSelectorIndex : 13;
            uint16_t reserved;
            
        };
    };
}ExceptionInfo;

typedef enum{
    Ring0 = 0,
    Ring1 = 1,
    Ring2 = 2,
    Ring3 = 3,
}InterruptPrivilegeLevel;

typedef struct{
   void (*handle)(void *);
   void *data;
   char *name;
}InterruptHandler;


//__attribute__((packed)) 

void interruptDescriptorTableInit();
uint8_t interrupt_setHandler(void (*handler)(void *), void *data, char *name);
uint8_t interrupt_setContinuousHandlers(InterruptHandler *handler, uint8_t handlerCount, bool aligned, char *name);

InterruptStatus interrupt_setExceptionHandler(
        void (*handler)(ExceptionInfo, void *),
        void *data,
        uint8_t vector
        );

InterruptStatus interrupt_setHardwareHandler(
        void (*interruptHandler)(void),
        uint8_t vector,
        InterruptPrivilegeLevel privilegeLevel);
#endif
