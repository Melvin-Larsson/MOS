#ifndef PCI_H_INCLUDED
#define PCI_H_INCLUDED

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

}PciHeader;

void pciConfigWriteAddress(uint32_t address);
void pci_configWriteData(uint32_t data);
void pci_configWrite(uint32_t address, uint32_t data);
uint32_t pci_configRead();
uint32_t pci_configReadAt(uint32_t address);
uint32_t pci_configReadDevice(uint8_t busNr, uint8_t deviceNr,
         uint8_t funcNr, uint8_t registerOffset);
void pci_printVendorIds();
int pci_getDevices(PciHeader* output, int maxHeadersInOutput);

#endif
