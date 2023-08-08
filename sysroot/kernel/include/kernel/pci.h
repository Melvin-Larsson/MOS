#ifndef PCI_H_INCLUDED
#define PCI_H_INCLUDED

#define PCI_CLASS_SERIAL_BUS_CONTROLLER 0xC
#define PCI_SUBCLASS_USB_CONTROLLER 0x3
#define PCI_PROG_IF_EHCI 0x10
#define PCI_PROG_IF_XHCI 0x30

#include "stdint.h"
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
         uint16_t command;
         uint16_t status;
         uint8_t revisionId;
         uint8_t progIf;
         uint8_t subclass;
         uint8_t classCode;
         uint8_t cacheLineSize;
         uint8_t latencyTimer;
         uint8_t headerType;
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
   PciHeader pciHeader;
   uint32_t baseAddress0;
   uint32_t baseAddress1;
   uint32_t baseAddress2;
   uint32_t baseAddress3;
   uint32_t baseAddress4;
   uint32_t baseAddress5;
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
}__attribute__((packed))PciGeneralDeviceHeader;

void pciConfigWriteAddress(uint32_t address);
void pci_configWriteData(uint32_t data);
void pci_configWrite(uint32_t address, uint32_t data);
uint32_t pci_configRead();
uint32_t pci_configReadAt(uint32_t address);
uint32_t pci_configReadDevice(uint8_t busNr, uint8_t deviceNr,
         uint8_t funcNr, uint8_t registerOffset);

int pci_getDevices(PciDescriptor* output, int maxHeadersInOutput);
int pci_getDevice(uint8_t classCode, uint8_t subclass, uint8_t progIf,
                  PciHeader* output, int maxHeadersInOutput);
void pci_getGeneralDevice(PciDescriptor* descriptor, PciGeneralDeviceHeader* output);
void pci_getClassName(PciHeader* pci, char* output);
void pci_getSubclassName(PciHeader* pci, char* output);
void pci_getProgIfName(PciHeader* pci, char* output);

#endif
