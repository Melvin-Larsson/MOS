#include "stdio.h"
#include "kernel/interrupt.h"
#include "kernel/apic.h"
#include "kernel/pci.h"
#include "kernel/xhcd.h"
#include "string.h"

char message[] = "Kernel started!\n";
static void printPciDevices(PciDescriptor *descriptors, int count){
    printf("%d devices detected:\n", count);
    for(int i = 0; i < count; i++){
        PciHeader *header = &(descriptors[i].pciHeader);
        char className[50];
        char subclassName[50];
        char progIfName[50];
        
        pci_getClassName(header, className);
        pci_getSubclassName(header, subclassName);
        pci_getProgIfName(header, progIfName);
        printf("%d: %s(%X) - %s(%X) - %s(%X)\n", i,
                className,header->classCode,
                subclassName, header->subclass,
                progIfName, header->progIf);
    }

}
static int getXhcdDevice(PciDescriptor* descriptors, int count,
                        PciGeneralDeviceHeader *output){
    for(int i = 0; i < count; i++){
        PciHeader *header = &(descriptors[i].pciHeader);

        if(header->classCode == PCI_CLASS_SERIAL_BUS_CONTROLLER
            && header->subclass == PCI_SUBCLASS_USB_CONTROLLER
            && header->progIf == PCI_PROG_IF_XHCI ){

            pci_getGeneralDevice(&descriptors[i], output);
            return 1;
        }
    }
    return 0;
}
static void initXhci(PciGeneralDeviceHeader xhc){
        Xhci xhci;
        if(xhcd_init(&xhc, &xhci) != 0){
            return;
        }
        printf("xhc initialized\n");
        int attachedPortNr = -1;
        while(attachedPortNr == -1){
            attachedPortNr = xhcd_checkForDeviceAttach(&xhci);
        }
        printf("Attatched port nr %d\n", attachedPortNr);
        if(!xhcd_enable(&xhci, attachedPortNr)){
           return;  
        }
        printf("USB device enabled\n");
        xhcd_initPort(&xhci, attachedPortNr);
}
void kernel_main(){
    stdioinit();
    printf(message);
    interruptDescriptorTableInit(); 

    printf("APIC present: %b\n", apic_isPresent());

    PciDescriptor devices[20];
    int count = pci_getDevices(devices, 10);
    printPciDevices(devices, count);
    PciGeneralDeviceHeader xhc;
    if(!getXhcdDevice(devices, count, &xhc)){
        printf("Error: Could not find xhc device!");
    }else{
        printf("Found a xhc device!\n");
        initXhci(xhc);
    }

    printf("end\n");
    while(1);
}
