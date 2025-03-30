#include "kernel/paging.h"
#include "kernel/pci.h"
#include "kernel/logging.h"
#include "kernel/msix-structures.h"
#include "kernel/interrupt.h"
#include "kernel/memory.h"
#include "string.h"
#include "stdlib.h"

#include "utils/assert.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define HEADER_TYPE_GENERAL_DEVICE 0x00
#define HEADER_TYPE_PCI_TO_PCI_BRIDGE 0x01
#define HEADER_TYPE_CARD_BUS_BRIDGE 0x02

#define APIC_EOI_ADDRESS 0xFEE000B0

typedef struct{
   void (*handler)(void *);
   void *data;
}InterruptData;

void pci_configWriteAddress(uint32_t address){
   __asm__ volatile("out %%eax, %%dx"
         :
         : [data]"eax"(address), [reg_addr]"dx"(CONFIG_ADDRESS)
         : 
         );
}
void pci_configWriteData(uint32_t data){
   __asm__ volatile("out %%eax, %%dx"
         :
         : [data]"a"(data), [reg_addr]"d"(CONFIG_DATA)
         : 
         );
}
void pci_configWrite(uint32_t address, uint32_t data){
   pci_configWriteAddress(address);
   pci_configWriteData(data);
}
uint32_t pci_configRead(){
   uint32_t data; 
   __asm__ volatile("in %%dx, %%eax"
                  : [out]"=a"(data)
                  : [addr]"d"(CONFIG_DATA)
                  : 
                  );
   return data;
}
uint32_t pci_configReadAt(uint32_t address){
   pci_configWriteAddress(address);
   return pci_configRead();
}
static uint32_t getAddress(uint8_t busNr, uint8_t deviceNr, uint8_t funcNr, uint8_t registerOffset){
   if(registerOffset & 0b11){
      loggError("Error: pci register offset has to point do a dword");
      return -1;
   }
   uint32_t busNrl = (uint32_t)busNr;
   uint32_t deviceNrl = (uint32_t)deviceNr;
   uint32_t funcNrl = (uint32_t)funcNr;
   uint32_t offsetl = (uint32_t)registerOffset;

   uint32_t address = (busNrl << 16) | (deviceNrl << 11) |
                      (funcNrl << 8) | (offsetl & 0xFC) | (1<<31);
   return address;

}
uint32_t pci_configReadRegister(uint8_t busNr, uint8_t deviceNr,
      uint8_t funcNr, uint8_t registerOffset){
   return pci_configReadAt(getAddress(busNr, deviceNr, funcNr, registerOffset));
}
void pci_configWriteRegister(uint8_t busNr, uint8_t deviceNr, uint8_t funcNr, uint8_t registerOffset, uint32_t value){
   pci_configWrite(getAddress(busNr, deviceNr, funcNr, registerOffset), value);
}

int pci_getDevices(PciDescriptor* output, int maxHeadersInOutput){
   int index = 0;
   
   for(uint8_t bus = 0; bus < 255; bus++){
      for(uint8_t device = 0; device < 32; device++){
         
         uint32_t reg0 = pci_configReadRegister(bus, device, 0, 0);
        
         if(reg0 != 0xFFFFFFFF){
            PciDescriptor *currDescriptor = &output[index];
            PciHeader *currHeader = &(currDescriptor->pciHeader);
            currHeader->reg0 = reg0;
            currHeader->reg1 = pci_configReadRegister(bus, device,0,0x4);
            currHeader->reg2 = pci_configReadRegister(bus, device,0,0x8);
            currHeader->reg3 = pci_configReadRegister(bus, device,0,0xc);


            currDescriptor->busNr = bus;
            currDescriptor->deviceNr = device;

            if(currHeader->classCode == 0xC &&
               currHeader->subclass == 0x3 &&
               currHeader->progIf == 0x30){
                  uint32_t reg1 = currHeader->reg1;
                  reg1 |= 1 << 10;
                  pci_configWriteRegister(bus, device, 0, 0x4, reg1);
                  currHeader->reg1 = pci_configReadRegister(bus, device,0,0x4);
                  loggDebug("regs: %X %X %X %X", currHeader->reg0, currHeader->reg1, currHeader->reg2, currHeader->reg3);
                  loggDebug("more debug");
               }
            
            loggDebug("1");
            index++;
            loggDebug("2");
            
            if(index >= maxHeadersInOutput){
            loggDebug("3");
               return index;
            }
            loggDebug("3");
         }
      }
   }
   loggDebug("Found %d devices", index);
   return index;
}
/*int pci_getDevice(uint8_t classCode, uint8_t subclass, uint8_t progIf,
                  PciHeader* output, int maxHeadersInOutput){

   return 0; //FIXME: not yet implemented

}*/
void pci_getGeneralDevice(const PciDescriptor descriptor,
                          PciGeneralDeviceHeader* output){
   output->pciHeader = descriptor.pciHeader;
   pci_configWriteRegister( //FIXME: temp
         descriptor.busNr,
         descriptor.deviceNr,
         0,
         4,
         (output->reg[1] | 1 << 2 | 1 << 1) & ~(1 | 1 << 10));
//    for(int i = 0; i < 4; i++){
//       printf("read %X ", output->reg[i]);
//    }

   for(int i = 0; i <= 0xF; i++){
      uint32_t val = 
       pci_configReadRegister(
            descriptor.busNr,
            descriptor.deviceNr,
            0,
            i * 4);
      output->reg[i] = val;
   }
   uint8_t addr = output->capabilitiesPointer & ~0b11;
   uint8_t next = 0;
   while(addr != 0){
      for(int i = 0; i < 24 / 4; i++){
          uint32_t val = pci_configReadRegister(
               descriptor.busNr,
               descriptor.deviceNr,
               0,
               addr + i * 4
               );
         if(i == 0){
             next = (val >> 8) & 0xFF;
         }
      }
      addr = next;
   }
}

PciStatus pci_getStatus(PciDescriptor* pci){
  uint32_t commandAndStatus = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x04); 
  uint16_t status = commandAndStatus >> 16;
  return (PciStatus){.status = status};
}

uint8_t pci_getCacheLineSize(PciDescriptor *pci){
   uint32_t reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x0C); 
   return reg & ~0xFF;
}
void pci_setCacheLineSize0xFEE000B0(PciDescriptor* pci, uint8_t cacheLineSize){
  uint32_t reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x0C); 
  reg &= ~0xFF;
  reg |= cacheLineSize;
  pci_configWriteRegister(pci->busNr, pci->deviceNr, 0, 0x0C, reg); 
}

uint8_t pci_getLatencyTimer(PciDescriptor *pci){
   uint32_t reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x0C); 
   return (reg >> 8) & ~0xFF;
}
void pci_setLatencyTimer(PciDescriptor* pci, uint8_t latencyTimer){
  uint32_t reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x0C); 
  reg &= ~0xFF00;
  reg |= (latencyTimer << 8);
  pci_configWriteRegister(pci->busNr, pci->deviceNr, 0, 0x0C, reg); 
}
/**
 * Invokes a built-in self test on the device
 * @param pci the device to test
 * @return 0 if successful or BIST not suported. -1 if device did not respond.
 * Other values are device specific.
 */
int pci_doBIST(PciDescriptor *pci){
   uint32_t reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x0C); 
   if(!(reg & (1<<31))){
      return 0;
   }
   reg |= 1 << 30;
   pci_configWriteRegister(pci->busNr, pci->deviceNr, 0, 0x0C, reg); 
   //FIMXE: Timer should be 2 seconds
   uint32_t timer = 1;
   while((reg & (1<<30)) && timer < 0xFFFFFFFF){
      timer++;
      reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x0C); 
   }
   if(timer == 0xFFFFFFFF){
      return -1;
   }
   return (reg >> 24) & 0xF;
}

static uint8_t getCapabilitiesPointer(const PciDescriptor *pci){
   assert(pci->pciHeader.headerType == HEADER_TYPE_GENERAL_DEVICE);
   assert(pci->pciHeader.status.capabilitiesList);

   uint32_t reg = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, 0x34);

   return (reg & 0xFF) & ~0b11; 
}
static uint8_t getNextPointer(uint16_t capabilitiesHeader){
   return (capabilitiesHeader >> 8) & ~0b11;
}
static uint8_t getCapabilitiesId(uint16_t capabilitiesHeader){
   return capabilitiesHeader & 0xFF;
}

int pci_searchCapabilityList(const PciDescriptor *pci, uint8_t id, PciCapability *result){
   assert(pci->pciHeader.headerType == HEADER_TYPE_GENERAL_DEVICE);

   if(!pci->pciHeader.status.capabilitiesList){
      return 0;
   }

   uint8_t offset = getCapabilitiesPointer(pci);
   if(offset == 0){
      return 0;
   }
   uint16_t capabilityHeader = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, offset) & 0xFFFF;
   while(getCapabilitiesId(capabilityHeader) != id){
      offset = getNextPointer(capabilityHeader);

      if(offset == 0){
         return 0;
      }

      capabilityHeader = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, offset) & 0xFFFF;
   }

   *result = (PciCapability){
      .id = id,
      .offset = offset
   };

   return 1;
}
int pci_readCapabilityData(const PciDescriptor *pci, PciCapability capability, void *result, int capabilitySize){
   assert(capabilitySize % 4 == 0); //Not necessarely true, leave here for now though

   uint8_t *result8 = result;
   uint8_t offset = capability.offset;

   while(capabilitySize > 0){
      uint32_t result32 = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, offset);
      memcpy(result8, &result32, sizeof(uint32_t));
      offset += 4;
      result8 += 4;
      capabilitySize -= 4;
   }

   return offset - capability.offset;
}

static uintptr_t getMessageTableBaseAddress(const PciDescriptor *pci, MsiXCapability capability){
   assert(pci->pciHeader.headerType == HEADER_TYPE_GENERAL_DEVICE);

   uint32_t barAddressOffset = 0x10 + capability.messageBir * 4;
   uint32_t address = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, barAddressOffset) & ~0xF;
   uint32_t offset = capability.tableOffsetHigh << 3;
   loggDebug("Message table offset %X", offset);
   return address + offset;
}
static uintptr_t getPendingTableBaseAddress(const PciDescriptor *pci, MsiXCapability capability){
   assert(pci->pciHeader.headerType == HEADER_TYPE_GENERAL_DEVICE);

   uint32_t barAddressOffset = 0x10 + capability.pendingBir * 4;
   uint32_t address = pci_configReadRegister(pci->busNr, pci->deviceNr, 0, barAddressOffset) & ~0xF;
   uint32_t offset = capability.pendingOffsetHigh << 3;
   loggDebug("Pending table offset %X", offset);
   return address + offset;
}

static uint64_t formatMsgAddr(uint32_t targetProcessor, int redirectionHint, int destinationMode){
   return (0xFEE << 20) | (targetProcessor << 12) | (redirectionHint != 0) << 3 | (destinationMode != 0) << 2;
}
static uint64_t formatMsgData(uint8_t vector, MsiDeliveryMode deliveryMode, int assert, int levelSensitive){
   return (levelSensitive != 0) << 15 | (assert != 0) << 14 | deliveryMode << 8 | vector;
}


int getCpuId(){
   uint32_t eax, ebx = 0, ecx = 0, edx = 0;
   eax = 0x01;
   __asm__ ("cpuid"
         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
         : "a"(eax)
         : 
         );
   return ebx >> 24;
}

int pci_isMsiXPresent(PciDescriptor pci){
   PciCapability capability;
   return pci_searchCapabilityList(&pci, 0x11, &capability);
}

int pci_initMsiX(const PciDescriptor *pci, MsiXDescriptor *result){
   assert(pci->pciHeader.headerType == HEADER_TYPE_GENERAL_DEVICE);

   PciCapability capability;
   int status = pci_searchCapabilityList(pci, 0x11, &capability);
   if(!status){
      loggError("MsiX not found");
      return 0;
   }
   MsiXCapability msiCapability = {0};
   pci_readCapabilityData(pci, capability, &msiCapability, sizeof(MsiXCapability));

   uintptr_t messageTablePhysical = getMessageTableBaseAddress(pci, msiCapability);
   uintptr_t pendingTablePhysical = getPendingTableBaseAddress(pci, msiCapability);

   *result = (MsiXDescriptor){
      .messageTable = messageTablePhysical,
      .pendingTable = pendingTablePhysical, 
      .tableSize = msiCapability.tableSize,
      .capability = capability
   };

   return 1;
}
int pci_enableMsiX(PciDescriptor pci, MsiXDescriptor msi){
   PciCapability capability = msi.capability;
   uint32_t reg = pci_configReadRegister(pci.busNr, pci.deviceNr, 0, capability.offset);
   reg |= (1 << 31);
   pci_configWriteRegister(pci.busNr, pci.deviceNr, 0, capability.offset, reg);
   return 1;
}
MsiXVectorData pci_getDefaultMsiXVectorData(void (*handler)(void *), void *data){
   return (MsiXVectorData){
      .handler = handler,
      .data = data,
      .targetProcessor = getCpuId(),
      .redirectionHint = 0,
      .destinationMode = 0,
      .levelSensitive = 0,
      .assert = 0,
      .deliveryMode = MsiDeliveryModeFixed
   };
}

#define MSI_CAPABILITY_ID 0x5
#define MSI_ENABLE_MASK (1 << 16)
#define MSI_PER_VECTOR_MASKING_CAPABLE_MASK (1 << 24)
#define MSI_64_BIT_CAPABLE_MASK (1 << 23)
#define MSI_MULTI_MESSAGE_ENABLE_MASK (0x700000)
#define MSI_MULTI_MESSAGE_ENABLE_POS (4 + 16)
#define MSI_MULTI_MESSAGE_CAPABLE_MASK (0xE0000)
#define MSI_MULTI_MESSAGE_CAPABLE_POS (1 + 16)

static uint32_t readCapabilityRegister(PciDescriptor descriptor, PciCapability capability, int offset){
   return pci_configReadRegister(descriptor.busNr, descriptor.deviceNr, 0, capability.offset + offset);
}
static void writeCapabilityRegister(PciDescriptor descriptor, PciCapability capability, int offset, uint32_t value){
  pci_configWriteRegister(descriptor.busNr, descriptor.deviceNr, 0, capability.offset + offset, value);
}

int pci_isMsiPresent(PciDescriptor pci){
   PciCapability capability;
   return pci_searchCapabilityList(&pci, MSI_CAPABILITY_ID, &capability);
}

MsiInitData pci_getDefaultSingleHandlerMsiInitData(void (*handler)(void *), void *data){
   return (MsiInitData){
      .handlers[0] = handler,
      .data[0] = data,
      .vectorCount = MsiVectorCount1,
      .targetProcessor = getCpuId(),
      .redirectionHint = 0,
      .destinationMode = 0,
      .levelSensitive = 0,
      .assert = 0,
      .deliveryMode = MsiDeliveryModeFixed
   };
}


static uint32_t getMultipleMessageEnableValue(MsiVectorCount enabledVectors){
   switch(enabledVectors){
      case MsiVectorCount1:
         return 0;
      case MsiVectorCount2:
         return 1;
      case MsiVectorCount4:
         return 2;
      case MsiVectorCount8:
         return 3;
      case MsiVectorCount16:
         return 4;
      case MsiVectorCount32:
         return 5;
      default:
         assert(0);
         return 0;
   }
}
static MsiVectorCount getVectorCount(int encoded){
   return 1 << encoded;
}

int pci_getMsiCapabilities(PciDescriptor pci, MsiCapabilities *result){
   PciCapability capability;
   int status = pci_searchCapabilityList(&pci, MSI_CAPABILITY_ID, &capability);
   if(!status){
      loggError("Msi not found");
      return 0;
   }

   uint32_t capabilityRegister = readCapabilityRegister(pci, capability, 0);
   *result = (MsiCapabilities){
      .is64BitAddressCapable = (capabilityRegister & MSI_64_BIT_CAPABLE_MASK ? 1 : 0),
      .isPerVectorMaskingCapable = (capabilityRegister & MSI_PER_VECTOR_MASKING_CAPABLE_MASK ? 1 : 0),
      .requestedVectors = getVectorCount((capabilityRegister & MSI_MULTI_MESSAGE_CAPABLE_MASK) >> MSI_MULTI_MESSAGE_CAPABLE_POS)
   };
   return 1;
}

void handler(void *data){
   InterruptData *interruptData = (InterruptData *)data;

   interruptData->handler(interruptData->data);

   uint32_t eoiData  = 1;
   paging_writePhysicalOfSize(APIC_EOI_ADDRESS, &eoiData, 4, AccessSize32);
}

int pci_initMsi(PciDescriptor pci, MsiDescriptor *result, MsiInitData data){
   InterruptHandler handlers[256];
   for(int i = 0; i < (int)data.vectorCount; i++){
      InterruptData *interruptData = kmalloc(sizeof(InterruptData));
      *interruptData = (InterruptData){
         .data = data.data[i],
         .handler = data.handlers[i]
      };
      handlers[i] = (InterruptHandler){
         .data = interruptData,
         .handle = handler,
      };
   }
   uint8_t startVector = interrupt_setContinuousHandlers(handlers, data.vectorCount, true, "Msi");
   if(startVector == 0){
      return 0;
   }

   PciCapability pciCapability;
   int status = pci_searchCapabilityList(&pci, MSI_CAPABILITY_ID, &pciCapability);
   if(!status){
      loggError("Msi not found");
      return 0;
   }

   MsiCapabilities msiCapabilities;
   pci_getMsiCapabilities(pci, &msiCapabilities);

   loggDebug("Caps, 64 bit: %d, mask: %d, encoded count %d", msiCapabilities.is64BitAddressCapable, msiCapabilities.isPerVectorMaskingCapable, msiCapabilities.requestedVectors);

   assert(data.vectorCount <= msiCapabilities.requestedVectors);

   uint64_t messageAddress = formatMsgAddr(
         data.targetProcessor,
         data.redirectionHint,
         data.destinationMode);

   uint16_t messageData = formatMsgData(
         startVector, 
         data.deliveryMode,
         data.assert,
         data.levelSensitive);


   uint32_t messageAddressOffset = 0x4;
   uint32_t messageDataOffset = msiCapabilities.is64BitAddressCapable ? 0xC : 0x8;

   loggDebug("MessageAddress %X (at offset %X)", (uint32_t)messageAddress, messageAddressOffset);
   loggDebug("MessageData MX (at offset %X)", (uint32_t)messageData, messageDataOffset);

   writeCapabilityRegister(pci, pciCapability, messageAddressOffset, (uint32_t)(messageAddress & 0xFFFFFFFF));
   if(msiCapabilities.is64BitAddressCapable){
      writeCapabilityRegister(pci, pciCapability, messageAddressOffset + 4, 0);
   }

   uint32_t messageDataRegister = readCapabilityRegister(pci, pciCapability, messageDataOffset);
   messageDataRegister &= ~0xFFFF;
   messageDataRegister |= (messageData & 0xFFFF);
   loggDebug("Message Mata register %X", messageDataRegister);
   writeCapabilityRegister(pci, pciCapability, messageDataOffset, messageDataRegister);

   uint32_t capabilityRegister = readCapabilityRegister(pci, pciCapability, 0);
   capabilityRegister &= ~MSI_MULTI_MESSAGE_ENABLE_MASK;
   capabilityRegister |= getMultipleMessageEnableValue(data.vectorCount) << MSI_MULTI_MESSAGE_ENABLE_POS
                      | MSI_ENABLE_MASK;
   loggDebug("Cap register %X", capabilityRegister);
   writeCapabilityRegister(pci, pciCapability, 0, capabilityRegister);

   if(msiCapabilities.isPerVectorMaskingCapable){
      uint32_t maskBitOffset = msiCapabilities.is64BitAddressCapable ? 0x10 : 0xC;
      writeCapabilityRegister(pci, pciCapability, maskBitOffset, 0);
   }


   *result = (MsiDescriptor){
      .msiCapabilities = msiCapabilities,
      .enabledVectors = data.vectorCount,
      .pciCapability = pciCapability,
      .pciDescriptor = pci
   };

   return 1;
}

static int andOrPendingBits(MsiDescriptor descriptor, uint32_t andValue, uint32_t orValue){
   assert(descriptor.msiCapabilities.isPerVectorMaskingCapable);

   uint32_t pendingBitRegisterOffset = descriptor.msiCapabilities.is64BitAddressCapable ? 0x14 : 0x10;
   uint32_t pendingBitRegister = readCapabilityRegister(
         descriptor.pciDescriptor,
         descriptor.pciCapability,
         pendingBitRegisterOffset);

   pendingBitRegister &= andValue;
   pendingBitRegister |= orValue;
   writeCapabilityRegister(
         descriptor.pciDescriptor,
         descriptor.pciCapability,
         pendingBitRegisterOffset,
         pendingBitRegister);

   return 1;
}

int pci_maskVector(MsiDescriptor descriptor, uint32_t vectorNumber){
   assert(vectorNumber < descriptor.enabledVectors);
   return andOrPendingBits(descriptor, ~0, 1 << vectorNumber);
}
int pci_unmaskVector(MsiDescriptor descriptor, uint32_t vectorNumber){
   assert(vectorNumber < descriptor.enabledVectors);
   return andOrPendingBits(descriptor, ~(1 << vectorNumber), 0);
}


//FIXME: Should not be allowed to specify interruptVectorNr
int pci_setMsiXVector(const MsiXDescriptor msix, int msiVectorNr, MsiXVectorData vectorData){
   //FIXME:!!Can not just create and forget here
   InterruptData *data = kmalloc(sizeof(InterruptData));
   *data = (InterruptData){
      .data = vectorData.data,
      .handler = vectorData.handler
   };
   uint8_t interruptVectorNr = interrupt_setHandler(handler, data, "msiX");
   if(interruptVectorNr == 0){
      return 0;
   }

   uintptr_t address = msix.messageTable + msiVectorNr * 2 * 8;
   uint64_t msgAddr = formatMsgAddr(
         vectorData.targetProcessor,
         vectorData.redirectionHint,
         vectorData.destinationMode);
   uint64_t msgData = formatMsgData(
      interruptVectorNr,
      vectorData.deliveryMode,
      vectorData.assert,
      vectorData.levelSensitive);

   paging_writePhysicalOfSize(address, &msgAddr, sizeof(msgAddr), AccessSize64);
   paging_writePhysicalOfSize(address + 8, &msgData, sizeof(msgAddr), AccessSize64);

   return 1;
}

void pci_getClassName(PciHeader* pci, char* output){
   char *names[] = {"Unclassified", "Mass Storage Controller",
      "Network Controller", "Display Controller", "Multimedia Controller", "Memory Controller",
      "Bridge", "Simple Communication Controller", "Base System Peripheral", "Input Device Controller",
      "Docking Station","Processor", "Serial Bus Controller", "Wireless Controller","Intelligent Controller",
      "Satellite Communication Controller", "Encrpytion Controller", "Processing Accelerator", "Non-Essential Instrumentation",
      "Reserved", "Co-Processor", "Reserved", "Vendor specific"
   };
   strcpy(output, names[pci->classCode]);
}
void pci_getSubclassName(PciHeader* pci, char* output){
   if(pci->classCode != PCI_CLASS_SERIAL_BUS_CONTROLLER){
      char subclass[10];
      sprintf(subclass, "%d", pci->subclass);
      strcpy(output, subclass);
      return;
   }
   /*static char *names[] = {"FireWire (IEEE 1394) Controller", "ACCESS Bus Controller", "SSA", "USB Controller",
                           "Fibre Channel", "SMBus Controller", "InfiniBand Controller", "IPMI Interface",
                           "SERCOS Interface", "CANbus Controller"}; */
   if(pci->subclass > 0x9){
      char invalid[] = "Invalid subclassCode";
      strcpy(output, invalid);
      return;
   }
  // strcpy(output, names[pci->subclass]);
     strcpy(output, "-");
}
void pci_getProgIfName(PciHeader* pci, char* output){
   if(pci->classCode != PCI_CLASS_SERIAL_BUS_CONTROLLER
   || pci->subclass != PCI_SUBCLASS_USB_CONTROLLER){
      char subclass[10];
      sprintf(subclass, "%d", pci->progIf);
      strcpy(output, subclass);
      return;
   }/*
   static char *names[] = {"UHCI Controller", "OHCI Controller", "EHCI (USB2) Controller", "XHCI (USB3) Controller"}; */
   if(pci->progIf >> 4 > 0x3){
      char invalid[] = "Invalid subclassCode or unspecified";
      strcpy(output, invalid);
      return;
   }
//   strcpy(output, names[pci->progIf >> 4]);
     strcpy(output, "-");
}
