#include "kernel/xhcd-hardware.h"
#include "kernel/paging.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

XhcHardware xhcd_initRegisters(PciGeneralDeviceHeader pciHeader){
   assert(pciHeader.baseAddress[1] == 0);
   XhcHardware xhcd;
   xhcd.capabilityBase = pciHeader.baseAddress[0] & (~0xFF);
   xhcd.operationalBase = xhcd.capabilityBase + xhcd_readCapability(xhcd, CAPLENGTH);
   xhcd.doorbellBase = xhcd.capabilityBase + (xhcd_readCapability(xhcd, DBOFF) & ~0x3);
   xhcd.runtimeBase = xhcd.capabilityBase + (xhcd_readCapability(xhcd, RTSOFF) & ~0x1F);
   return xhcd;
}

static int getOpertionalRegisterSize(XhcOperationalRegister xhcRegister){
   if(xhcRegister == CRCR || xhcRegister == DCBAAP){
      return 8;
   }
   return 4;
}
static AccessSize getOpertionalRegisterAccessSize(XhcOperationalRegister xhcRegister){
   if(xhcRegister == CRCR || xhcRegister == DCBAAP){
      return AccessSize64;
   }
   return AccessSize32;
}

void xhcd_writeRegister(XhcHardware xhc, XhcOperationalRegister xhcRegister, uint64_t data){
   paging_writePhysicalOfSize(
         xhc.operationalBase + xhcRegister,
         &data,
         getOpertionalRegisterSize(xhcRegister),
         getOpertionalRegisterAccessSize(xhcRegister));
}
uint64_t xhcd_readRegister(XhcHardware xhc, XhcOperationalRegister xhcRegister){
   uint64_t result = 0;
   paging_readPhysicalOfSize(
         xhc.operationalBase + xhcRegister,
         &result,
         getOpertionalRegisterSize(xhcRegister),
         getOpertionalRegisterAccessSize(xhcRegister));
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

   paging_writePhysicalOfSize(xhc.operationalBase + 0x400 + port * 16 + portRegister, &data, 4, AccessSize32);
}
uint32_t xhcd_readPortRegister(
      XhcHardware xhc,
      uint32_t port,
      XhcPortRegister portRegister){

   uint32_t result;
   paging_readPhysicalOfSize(xhc.operationalBase + 0x400 + port * 16 + portRegister, &result, 4, AccessSize32);
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
static AccessSize getCapabilityAccessSize(XhcCapabilityRegister capabilityRegister){
   AccessSize accessSize = AccessSize32;
   if(capabilityRegister == CAPLENGTH){
      accessSize = AccessSize8;
   }else if(capabilityRegister == HCIVERSION){
      accessSize = AccessSize16;
   }
   return accessSize;
}
void xhcd_writeCapability(XhcHardware xhc, XhcCapabilityRegister capabilityRegister, uint32_t data){
   paging_writePhysicalOfSize(
         xhc.capabilityBase + capabilityRegister,
         &data,
         getCapabilitySize(capabilityRegister),
         getCapabilityAccessSize(capabilityRegister));
}
uint32_t xhcd_readCapability(XhcHardware xhc, XhcCapabilityRegister capabilityRegister){
   uint32_t result = 0;
   paging_readPhysicalOfSize(
         xhc.capabilityBase + capabilityRegister,
         &result,
         getCapabilitySize(capabilityRegister),
         getCapabilityAccessSize(capabilityRegister));
   return result;
}
static uint32_t getInterruptorRegSize(XhcInterruptorRegister interruptorRegister){
   if(interruptorRegister == ERSTBA || interruptorRegister == ERDP){
      return 8;
   }
   return 4;
}
static uint32_t getInterruptorAccessSize(XhcInterruptorRegister interruptorRegister){
   if(interruptorRegister == ERSTBA || interruptorRegister == ERDP){
      return AccessSize64;
   }
   return AccessSize32;
}
void xhcd_writeInterrupter(XhcHardware xhc, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t value){
   paging_writePhysicalOfSize(
         xhc.runtimeBase + 0x20 + index * 32 + interruptorRegister,
         &value,
         getInterruptorRegSize(interruptorRegister),
         getInterruptorAccessSize(interruptorRegister));
}
uint64_t xhcd_readInterrupter(XhcHardware xhc, uint8_t index, XhcInterruptorRegister interruptorRegister){
   uint64_t result = 0;
   paging_readPhysicalOfSize(
         xhc.runtimeBase + 0x20 + index * 32 + interruptorRegister,
         &result,
         getInterruptorRegSize(interruptorRegister),
         getInterruptorAccessSize(interruptorRegister));

   return result;
}
void xhcd_orInterrupter(XhcHardware xhc, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t orValue){
   uint64_t value = xhcd_readInterrupter(xhc, index, interruptorRegister);
   xhcd_writeInterrupter(xhc, index, interruptorRegister, value | orValue);
}
void xhcd_andInterrupter(XhcHardware xhc, uint16_t index, XhcInterruptorRegister interruptorRegister, uint64_t andValue){
   uint64_t value = xhcd_readInterrupter(xhc, index, interruptorRegister);
   xhcd_writeInterrupter(xhc, index, interruptorRegister, value & andValue);
}


void xhcd_writeDoorbell(XhcHardware xhc, uint8_t index, uint32_t value){
   paging_writePhysicalOfSize(xhc.doorbellBase + index * 4, &value, 4, AccessSize32);
}
uint32_t xhcd_readDoorbell(XhcHardware xhc, uint8_t index){
   uint32_t result;
   paging_readPhysicalOfSize(xhc.doorbellBase + index * 4, &result, 4, AccessSize32);
   return result;
}

XhcExtendedCapabilityEnumerator xhcd_newExtendedCapabilityEnumerator(XhcHardware xhc){
   uint32_t xECP = (xhcd_readCapability(xhc, HCCPARAMS1) >> 16) << 2;
   if(!xECP){
      return (XhcExtendedCapabilityEnumerator){.data = 0};
   }

   return (XhcExtendedCapabilityEnumerator){
      .data = (void*)(xECP + xhc.capabilityBase)
   };
}

void xhcd_advanceExtendedCapabilityEnumerator(XhcExtendedCapabilityEnumerator *enumerator){
   assert(enumerator->data != 0);

   uint32_t extendedCapability;
   paging_readPhysicalOfSize((uintptr_t)enumerator->data, &extendedCapability, 4, AccessSize32);

   uint32_t offset = ((extendedCapability >> 8) & 0xFF) << 2;
   if(offset == 0){
      enumerator->data = 0;
   }else{
      uintptr_t lastAddress = (uintptr_t)enumerator->data;
      enumerator->data = (void*)(lastAddress + offset);
   }
}
void xhcd_readExtendedCapability(XhcExtendedCapabilityEnumerator *enumerator, void *result, int size){
   assert(enumerator->data != 0);
   assert(size % 4 == 0);

   paging_readPhysicalOfSize((uintptr_t)enumerator->data, result, size, AccessSize32);
}
void xhcd_writeExtendedCapability(XhcExtendedCapabilityEnumerator *enumerator, void *data, int size){
   assert(enumerator->data != 0);
   assert(size % 4 == 0);

   paging_writePhysicalOfSize((uintptr_t)enumerator->data, data, size, AccessSize32);
}

int xhcd_hasNextExtendedCapability(XhcExtendedCapabilityEnumerator *enumerator){
   return enumerator->data != 0;
}
