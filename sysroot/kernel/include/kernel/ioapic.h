#ifndef IO_APIC_H_INCLUDED
#define IO_APIC_H_INCLUDED

#include "stdint.h"
#include "stdbool.h"

typedef enum{
   Fixed = 0,
   LowestPriority = 1,
   SMI = 2,
   NMI = 4,
   INIT = 5,
   ExtInt = 7
}IOApicDeliveryMode;

typedef struct{
   uint8_t destination;
   bool mask;
   bool levelSensitive;
   bool useActiveLow;
   bool logicalDestinationMode;
   IOApicDeliveryMode deliveryMode;
   uint8_t interruptVector;
}IRQConfig;


void ioapic_init();

IRQConfig ioapic_getDefaultIRQConfig(uint8_t destinationAPIC, uint8_t interruptVector);
void ioapic_configureIrq(uint8_t irqNumber, IRQConfig config);


#endif
