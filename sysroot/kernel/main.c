#include "stdio.h"
#include "kernel/interrupt.h"
#include "kernel/apic.h"
#include "kernel/pci.h"
#include "string.h"

char message[] = "Kernel started!\n";
void kernel_main(){
    stdioinit();
    printf(message);
    interruptDescriptorTableInit(); 

    printf("APIC present: %b\n", apic_isPresent());
    //pci_printVendorIds();

    PciHeader devices[20];
    int count = pci_getDevices(devices, 10);
    printf("%d devices connected:\n", count);
    for(int i = 0; i < count; i++){
        char className[50];
        char subclassName[50];
        char progIfName[50];
        
        pci_getClassName(&devices[i], className);
        pci_getSubclassName(&devices[i], subclassName);
        pci_getProgIfName(&devices[i], progIfName);
        printf("%d: %s - %s - %s\n", i, className, subclassName, progIfName);
    }

    printf("end\n");
    while(1);
}
