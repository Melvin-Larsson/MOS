#include "kernel/pci.h"
#include "stdio.h"
#include "string.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

void pci_configWriteAddress(uint32_t address){
   __asm__ volatile("out %[data], %[reg_addr]"
         :
         : [data]"a"(address), [reg_addr]"d"(CONFIG_ADDRESS)
         : 
         );
}
void pci_configWriteData(uint32_t data){
   __asm__ volatile("out %[data], %[reg_addr]"
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
   __asm__ volatile("in %[addr], %[out]"
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
      printf("Error: pci register offset has to point do a dword");
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
           // printf("%d\n",reg0);
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
                  printf("r1: %X\n", reg1);
                  pci_configWriteRegister(bus, device, 0, 0x4, reg1);
                  currHeader->reg1 = pci_configReadRegister(bus, device,0,0x4);
                  printf("regs: %X %X %X %X\n", currHeader->reg0, currHeader->reg1, currHeader->reg2, currHeader->reg3);
               }
            
            index++;
            
            if(index >= maxHeadersInOutput){
               return index;
            }
         }
      }
   }
   return index;
}
/*int pci_getDevice(uint8_t classCode, uint8_t subclass, uint8_t progIf,
                  PciHeader* output, int maxHeadersInOutput){

   return 0; //FIXME: not yet implemented

}*/
void pci_getGeneralDevice(PciDescriptor* descriptor,
                          PciGeneralDeviceHeader* output){
   output->pciHeader = descriptor->pciHeader;
   pci_configWriteRegister( //FIXME: temp
         descriptor->busNr,
         descriptor->deviceNr,
         0,
         4,
         (output->reg[1] | 1 << 2 | 1 << 1) & ~(1 | 1 << 10));
//    for(int i = 0; i < 4; i++){
//       printf("read %X ", output->reg[i]);
//    }

   printf("read: ");
   for(int i = 0; i <= 0xF; i++){
      uint32_t val = 
       pci_configReadRegister(
            descriptor->busNr,
            descriptor->deviceNr,
            0,
            i * 4);
      output->reg[i] = val;
      printf("%X ", val);
   }
   printf("\n");
   uint8_t addr = output->capabilitiesPointer & ~0b11;
   printf("\naddr: %X\n", addr);
   uint8_t next = 0;
   while(addr != 0){
      for(int i = 0; i < 24 / 4; i++){
          uint32_t val = pci_configReadRegister(
               descriptor->busNr,
               descriptor->deviceNr,
               0,
               addr + i * 4
               );
         if(i == 0){
             next = (val >> 8) & 0xFF;
         }
         printf("-%X ", val);
      }
      printf("\n");
      addr = next;

   }

      printf("\n");

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
