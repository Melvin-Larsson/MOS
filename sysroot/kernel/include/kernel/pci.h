#ifndef PCI_H_INCLUDED
#define PCI_H_INCLUDED

#define PCI_CLASS_SERIAL_BUS_CONTROLLER 0xC
#define PCI_SUBCLASS_USB_CONTROLLER 0x3
#define PCI_PROG_IF_EHCI 0x10
#define PCI_PROG_IF_XHCI 0x30

#include "stdint.h"


typedef enum{
   MsiDeliveryModeFixed = 0b000,
   MsiDeliveryModeLowestPriority = 0b001,
   MsiDeliveryModeSMI = 0b010,
   MsiDeliveryModeNMI = 0b100,
   MsiDeliveryModeInit = 0b101,
   MsiDeliveryModeExtInt = 0b111,
}MsiDeliveryMode;

typedef struct{
   void (*handler)(void *data);
   void *data;

   uint8_t targetProcessor;
   int redirectionHint;
   int destinationMode;

   MsiDeliveryMode deliveryMode;
   int levelSensitive;
   int assert;
}MsiXVectorData;


typedef union{
   struct{
      uint16_t reserved3: 3;
      uint16_t interruptStatus: 1;
      uint16_t capabilitiesList: 1;
      uint16_t capable66MHz: 1;
      uint16_t reserved4: 1;
      uint16_t fastBackToBackCapable: 1;
      uint16_t masterDataParityError: 1;
      uint16_t devselTiming: 2;
      uint16_t signaledTargetAbort: 1;
      uint16_t receivedTargetAbort: 1;
      uint16_t receivedMasterAbort: 1;
      uint16_t signaledSystemError: 1;
      uint16_t detectedParityError: 1;
   };
   uint16_t status;
}__attribute__((packed)) PciStatus;

typedef struct{
   union{
      struct{
         uint32_t reg0;
         uint32_t reg1;
         uint32_t reg2;
         uint32_t reg3;
      };
      struct{
         uint16_t vendorId;
         uint16_t deviceId;
         union{
            uint16_t command;
            struct{
               uint16_t ioSpace: 1;
               uint16_t memorySpace: 1;
               uint16_t busMaster: 1;
               uint16_t specialCycles: 1;
               uint16_t memoryWriteInvalidate: 1;
               uint16_t vgaPaletteSnoop: 1;
               uint16_t parityErrorResponse: 1;
               uint16_t reserved1: 1;
               uint16_t serrEnable: 1;
               uint16_t fastBackToBackEnable: 1;
               uint16_t interruptDisable: 1;
               uint16_t reserved2: 5;
            };
         };
         PciStatus status;
         uint8_t revisionId;
         uint8_t progIf;
         uint8_t subclass;
         uint8_t classCode;
         uint8_t cacheLineSize;
         uint8_t latencyTimer;
         uint8_t headerType : 7;
         uint8_t multiFunctionDevice : 1;
         uint8_t BIST;
      };
   };

}__attribute__((packed))PciHeader;

typedef struct{
   uint8_t busNr;
   uint8_t deviceNr;
   PciHeader pciHeader;
}PciDescriptor;

typedef struct{
   union{
      struct{
         PciHeader pciHeader;
         uint32_t baseAddress[6];
         uint32_t cardbusCisPointer;
         uint16_t subsystemVendorId;
         uint16_t subsytemId;
         uint32_t expansionROMBaseAddress;
         uint32_t capabilitiesPointer : 8;
         uint32_t reserved : 24;
         uint32_t reserved2;
         uint8_t interruptLine;
         uint8_t interruptPin;
         uint8_t minGrant;
         uint8_t maxLatency;
      };
      uint32_t reg[16];
   };
}__attribute__((packed))PciGeneralDeviceHeader;

typedef struct{
   uint32_t offset;
   uint32_t id;
}PciCapability;

typedef struct{
   uintptr_t messageTable;
   uintptr_t pendingTable;
   uint16_t tableSize;
   PciCapability capability;
}MsiXDescriptor;


typedef enum{
   MsiVectorCount1 = 1,
   MsiVectorCount2 = 2,
   MsiVectorCount4 = 4,
   MsiVectorCount8 = 8,
   MsiVectorCount16 = 16,
   MsiVectorCount32 = 32,
}MsiVectorCount;

typedef struct{
   void (*handlers[32])(void *data);
   void *data[32];
   MsiVectorCount vectorCount;

   uint8_t targetProcessor;
   int redirectionHint;
   int destinationMode;

   MsiDeliveryMode deliveryMode;
   int levelSensitive;
   int assert;
}MsiInitData;

typedef struct{
   int isPerVectorMaskingCapable;
   int is64BitAddressCapable;
   MsiVectorCount requestedVectors;
}MsiCapabilities;

typedef struct{
   MsiCapabilities msiCapabilities;
   MsiVectorCount enabledVectors;
   PciCapability pciCapability;
   PciDescriptor pciDescriptor;
}MsiDescriptor;

void pciConfigWriteAddress(uint32_t address);
void pci_configWriteData(uint32_t data);
void pci_configWrite(uint32_t address, uint32_t data);
uint32_t pci_configRead();
uint32_t pci_configReadAt(uint32_t address);
uint32_t pci_configReadDevice(uint8_t busNr, uint8_t deviceNr,
         uint8_t funcNr, uint8_t registerOffset);

int pci_getDevices(PciDescriptor* output, int maxHeadersInOutput);

PciStatus pci_getStatus(PciDescriptor* pci);
uint8_t pci_getCacheLineSize(PciDescriptor *pci);
void pci_setCacheLineSize(PciDescriptor* pci, uint8_t cacheLineSize);
uint8_t pci_getLatencyTimer(PciDescriptor *pci);
void pci_setLatencyTimer(PciDescriptor* pci, uint8_t latencyTimer);

/**
 * Invokes a built-in self test on the device
 * @param pci the device to test
 * @return 0 if successful or BIST not suported. -1 if device did not respond.
 * Other values are device specific.
 */
int pci_doBIST(PciDescriptor *pci);
int pci_searchCapabilityList(const PciDescriptor *pci, uint8_t id, PciCapability *result);
int pci_readCapabilityData(const PciDescriptor *pci, PciCapability capability, void *result, int capabilitySize);

int pci_isMsiXPresent(PciDescriptor pci);
int pci_initMsiX(const PciDescriptor *pci, MsiXDescriptor *result);
int pci_enableMsiX(PciDescriptor pci, MsiXDescriptor msi);
MsiXVectorData pci_getDefaultMsiXVectorData(void (*handler)(void *), void *data);
int pci_setMsiXVector(const MsiXDescriptor msix, int msiVectorNr, int interruptVectorNr, MsiXVectorData vectorData);

int pci_isMsiPresent(PciDescriptor pci);
MsiInitData pci_getDefaultSingleHandlerMsiInitData(void (*handler)(void *), void *data);
int pci_getMsiCapabilities(PciDescriptor pci, MsiCapabilities *result);
int pci_initMsi(PciDescriptor pci, MsiDescriptor *result, MsiInitData data, uint8_t startVector);

void pci_getGeneralDevice(const PciDescriptor descriptor, PciGeneralDeviceHeader* output);
void pci_getClassName(PciHeader* pci, char* output);
void pci_getSubclassName(PciHeader* pci, char* output);
void pci_getProgIfName(PciHeader* pci, char* output);

#endif
