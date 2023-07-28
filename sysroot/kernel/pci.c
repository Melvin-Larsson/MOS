#include "kernel/pci.h"
#include "stdio.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

void pci_configWriteAddress(uint32_t address){
   __asm__ volatile(" \
         mov $0xCF8, %%dx \n\
         mov %0, %%eax      \n\
         out %%eax, %%dx"
         :
         : "m"(address)
         : "%dx", "%eax");
}
void pci_configWriteData(uint32_t data){
   __asm__ volatile(" \
         mov $0xCFC, %%dx \n\
         mov %0, %%eax      \n\
         out %%eax, %%dx"
         :
         : "m"(data)
         : "%dx", "%eax");

}
void pci_configWrite(uint32_t address, uint32_t data){
   pci_configWriteAddress(address);
   pci_configWriteData(data);
}
uint32_t pci_configRead(){
   uint32_t data; 
   __asm__ volatile("\
            mov $0xCFC, %%dx \n\
            in %%dx, %%eax  \n\
            mov %%eax, %0" 
            : "=r"(data)
            :
            : "%dx", "%eax"); //FIXME: merge step 2 and 3
   return data;
}
uint32_t pci_configReadAt(uint32_t address){
   pci_configWriteAddress(address);
   return pci_configRead();
}
uint32_t pci_configReadRegister(uint8_t busNr, uint8_t deviceNr,
      uint8_t funcNr, uint8_t registerOffset){

   uint32_t address = (busNr << 16) | (deviceNr << 11) |
                      (funcNr << 8) | (registerOffset) | (1<<31);
   return pci_configReadAt(address);
}

void pci_printVendorIds(){
   printf("Printing pci devices:\n");
   for(uint8_t bus = 0; bus < 255; bus++){
      for(uint8_t device = 0; device < 32; device++){
         uint32_t reg = pci_configReadRegister(bus, device, 0, 0);
         if(reg != 0xFFFFFFFF){
            uint16_t vendorId = reg & 0xFFFF;
            printf("%d\n", vendorId);
         }
      }
   }
}
int pci_getDevices(PciHeader* output, int maxHeadersInOutput){
   int index = 0;
   
   for(uint8_t bus = 0; bus < 255; bus++){
      for(uint8_t device = 0; device < 32; device++){
         
         uint32_t reg0 = pci_configReadRegister(bus, device, 0, 0);
        
         if(reg0 != 0xFFFFFFFF){
            PciHeader *curr = &output[index];
           // printf("%d\n",reg0);
            curr->reg0 = reg0;
            curr->reg1 = pci_configReadRegister(bus, device,0,0x4);
            curr->reg2 = pci_configReadRegister(bus, device,0,0x8);
            curr->reg3 = pci_configReadRegister(bus, device,0,0xc);
            
            
            index++;
            
            if(index >= maxHeadersInOutput){
               return index;
            }
         }
      }
   }
   return index;
}

