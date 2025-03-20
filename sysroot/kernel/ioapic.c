#include "kernel/ioapic.h"
#include "kernel/acpi.h"
#include "kernel/paging.h"

#include "utils/assert.h"

typedef union{
   struct{
      uint64_t interruptVector: 8;
      uint64_t deliveryMode : 3;
      uint64_t destinationMode : 1;
      uint64_t deliveryStatus : 1;
      uint64_t interruptPinPolarity : 1;
      uint64_t remoteIrr : 1;
      uint64_t triggerMode : 1;
      uint64_t interruptMask : 1;
      uint64_t reserved : 39;
      uint64_t destination : 8;
   };
   uint32_t words[2];
}RedirectionTableEntry;

static uintptr_t ioRegSel;
static uintptr_t ioWin;


void ioapic_init(){
   IoAcpiData ioAcpi;
   assert(acpi_getIOApicData(&ioAcpi));

   loggDebug("IoRegSel at %X", ioAcpi.address);
   ioRegSel = ioAcpi.address;
   ioWin = ioAcpi.address + 0x10;
}

IRQConfig ioapic_getDefaultIRQConfig(uint8_t destinationAPIC, uint8_t interruptVector){
   return (IRQConfig){
      .destination = destinationAPIC,
      .mask = false,
      .levelSensitive = false,
      .useActiveLow = false,
      .logicalDestinationMode = false,
      .deliveryMode = Fixed,
      .interruptVector = interruptVector
   };
}

void ioapic_configureIrq(uint8_t irqNumber, IRQConfig config){
   if(!assert(irqNumber <= 23)){
      return;
   }

   RedirectionTableEntry entry = {
      .interruptVector = config.interruptVector,
      .deliveryMode = config.deliveryMode,
      .destinationMode = config.logicalDestinationMode ? 1 : 0,
      .interruptPinPolarity = config.useActiveLow ? 1 : 0,
      .triggerMode = config.levelSensitive ? 1 : 0,
      .interruptMask = config.mask ? 1 : 0,
      .destination = config.destination
   };

   uint32_t address = 0x10 + irqNumber * 2;
   for(int i = 0; i < 2; i++){
      uint32_t regsel = address + i;
      paging_writePhysicalOfSize(ioRegSel, &regsel, sizeof(uint32_t), AccessSize32);
      paging_writePhysicalOfSize(ioWin, &entry.words[i], sizeof(uint32_t), AccessSize32);
   }
}
