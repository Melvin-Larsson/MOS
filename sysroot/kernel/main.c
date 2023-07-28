#include "stdio.h"
#include "kernel/interrupt.h"
#include "kernel/apic.h"
#include "kernel/pci.h"

char message[] = "Kernel started!\n";
void kernel_main(){
    stdioinit();
    printf(message);
    interruptDescriptorTableInit(); 

    printf("APIC present: %d\n", apic_isPresent());
    //pci_printVendorIds();

    PciHeader devices[20];
    int count = pci_getDevices(devices, 10);
    printf("%d devices connected\n", count);
    for(int i = 0; i < count; i++){
        printf("%d\n", devices[i].classCode);
    }

    printf("end\n");
    while(1);
}
