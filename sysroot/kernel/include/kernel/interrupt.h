
#ifndef INTERRUPT_H_INCLUDED
#define INTERRUPT_H_INCLUDED

#include "stdint.h"

typedef struct{
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

//__attribute__((packed)) 

void interruptDescriptorTableInit();
#endif
