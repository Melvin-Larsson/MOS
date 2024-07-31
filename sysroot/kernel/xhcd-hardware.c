#include "kernel/xhcd-hardware.h"
#include "kernel/paging.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

XhcHardware xhcd_initRegisters(PciGeneralDeviceHeader pciHeader){
   assert(pciHeader.baseAddress[1] == 0);
   XhcHardware xhcd;
   xhcd.capabilityBase = pciHeader.baseAddress[0] & (~0xFF);
   xhcd.operationalBase = xhcd.capabilityBase + xhcd_readCapability(xhcd, CAPLENGTH);
   printf("init length %X\n", xhcd_readCapability(xhcd, CAPLENGTH));
   printf("init address %X\n", xhcd.capabilityBase);
   printf("hcsparams1 %X\n", xhcd_readCapability(xhcd, HCSPARAMS1));
   while(1);
   xhcd.doorbellBase = xhcd.capabilityBase + xhcd_readCapability(xhcd, DBOFF);
   xhcd.runtimeBase = xhcd.capabilityBase + xhcd_readCapability(xhcd, RTSOFF);
   return xhcd;
}

void xhcd_writeRegister(XhcHardware xhc, XhcOperationalRegister xhcRegister, uint32_t data){
   paging_writePhysical(xhc.operationalBase + xhcRegister, &data, 4);
}
uint32_t xhcd_readRegister(XhcHardware xhc, XhcOperationalRegister xhcRegister){
   printf("address %X\n", xhc.operationalBase);
   uint32_t result;
   paging_readPhysical(xhc.operationalBase + xhcRegister, &result, 4);
   return result;
}
void xhcd_orRegister(XhcHardware xhc, XhcOperationalRegister xhcdRegister, uint32_t orValue){
   uint32_t currValue = xhcd_readRegister(xhc, xhcdRegister); 
   xhcd_writeRegister(xhc, xhcdRegister, currValue | orValue);
}
void xhcd_andRegister(XhcHardware xhc, XhcOperationalRegister xhcdRegister, uint32_t andValue){
   uint32_t currValue = xhcd_readRegister(xhc, xhcdRegister); 
   xhcd_writeRegister(xhc, xhcdRegister, currValue & andValue);
}

void xhcd_writePortRegister(
      XhcHardware xhc,
      uint32_t port,
      XhcPortRegister portRegister,
      uint32_t data){

   paging_writePhysical(xhc.operationalBase + port * 16 + portRegister, &data, 4);
}
uint32_t xhcd_readPortRegister(
      XhcHardware xhc,
      uint32_t port,
      XhcPortRegister portRegister){

   uint32_t result;
   paging_readPhysical(xhc.operationalBase + port * 16 + portRegister, &result, 4);
   return result;
}

static uint32_t getCapabilitySize(XhcCapabilityRegister capabilityRegister){
   uint32_t size = 4;
   if(capabilityRegister == CAPLENGTH){
      size = 1;
   }else if(capabilityRegister == HCIVERSION){
      size = 2;
   }
   return size;
}
void xhcd_writeCapability(XhcHardware xhc, XhcCapabilityRegister capabilityRegister, uint32_t data){
   paging_writePhysical(xhc.capabilityBase + capabilityRegister, &data, getCapabilitySize(capabilityRegister));
}
uint32_t xhcd_readCapability(XhcHardware xhc, XhcCapabilityRegister capabilityRegister){
   uint32_t result = 0;
   printf("size %d\n", getCapabilitySize(capabilityRegister));
   paging_readPhysical(xhc.capabilityBase + capabilityRegister, &result, getCapabilitySize(capabilityRegister));
   return result;
}
static uint32_t getInterruptorRegSize(XhcInterruptorRegister interruptorRegister){
   if(interruptorRegister == ERSTBA || interruptorRegister == ERDP){
      return 8;
   }
   return 4;
}
void xhcd_writeInterrupter(XhcHardware xhc, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t value){
   uint32_t size = getInterruptorRegSize(interruptorRegister);
   paging_writePhysical(xhc.runtimeBase + 0x20 + index * 32 + interruptorRegister, &value, size);
}
uint64_t xhcd_readInterrupter(XhcHardware xhc, uint8_t index, XhcInterruptorRegister interruptorRegister){
   uint64_t result = 0;
   uint32_t size = getInterruptorRegSize(interruptorRegister);
   paging_writePhysical(xhc.runtimeBase + 0x20 + index * 32 + interruptorRegister, &result, size);
   return result;
}

void xhcd_writeDoorbell(XhcHardware xhc, uint8_t index, uint32_t value){
   paging_writePhysical(xhc.doorbellBase + index * 4, &value, 4);
}
uint32_t xhcd_readDoorbell(XhcHardware xhc, uint8_t index){
   uint32_t result;
   paging_readPhysical(xhc.doorbellBase + index * 4, &result, 4);
   return result;
}
