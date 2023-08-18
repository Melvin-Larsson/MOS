#include "stdio.h"
#include "stdlib.h"
#include "kernel/interrupt.h"
#include "kernel/apic.h"
#include "kernel/pci.h"
#include "kernel/xhcd.h"
#include "kernel/keyboard.h"
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
            printf("Failed to enable port\n");
           return;  
        }
        printf("USB device enabled\n");
        int slotId = xhcd_initPort(&xhci, attachedPortNr);
        UsbDeviceDescriptor descriptor;
        xhcd_getDeviceDescriptor(&xhci, slotId, &descriptor);
        printf("Number of configurations %d\n", descriptor.bNumConfigurations);
        UsbConfiguration *config = xhcd_getConfiguration(&xhci, slotId, 0);
        UsbInterfaceDescriptor interfaceDescriptor = config->interfaces[0].descriptor;
        printf("interface class %d\n", interfaceDescriptor.bInterfaceClass);
        printf("interface subclass %d\n", interfaceDescriptor.bInterfaceSubClass);
        printf("interface protocol %d\n", interfaceDescriptor.bInterfaceProtocol);
        if(interfaceDescriptor.bInterfaceClass == 3){
            UsbDevice device = {slotId, config};
            KeyboardStatus status = keyboard_init(&xhci, &device);
            char buffer[100];
            keyboard_getStatusCode(status, buffer);
            printf("%s\n", buffer);
        }
        xhcd_freeConfiguration(config);
}
static int overlaps(uint8_t *p1, int s1, uint8_t *p2, int s2){
    return p1 + s1 > p2 && p1 < p2 + s2;

}
static void testMemory(){
    uint8_t *m1 = malloc(10);
    uint8_t *m2 = malloc(10);
    uint8_t *m3 = malloc(10);
    if(overlaps(m1, 10, m2, 10) || overlaps(m2, 10, m3, 10) || overlaps(m1, 10, m3, 10)){
        printf("Memory check failed, memories overlap!\n");
        while(1);
    }
    free(m2);
    uint8_t *m4 = malloc(10);
    if(m2 != m4){
        printf("Memory check failed, failed to reuse memory(1)!\n");
        while(1);
    }
    free(m4);
    uint8_t *m5 = malloc(1);
    uint8_t *m6 = malloc(1);
    if(!overlaps(m4, 10, m5, 1) || !overlaps(m4, 10, m6, 1)){
        printf("Memory check failed, failed to reuse memory(2)!\n");
        while(1);
    }
    free(m1);
    free(m3);
    free(m5);
    free(m6);
    uint8_t *m7 = malloc(10);
    if(m7 != m1){
        printf("Memory check failed, failed to reuse memory(3)!\n");
        while(1);
    }
    free(m7);
    printf("Passed memory check!\n");
}
static void testMemoryConstrained(){
    uint8_t *m1 = mallocco(10, 0x100, 0);
    if((uintptr_t)m1 % 0x100 != 0){
        printf("Constrained memory check failed, failed to align!\n");
        while(1);
    }
    uint8_t *m2 = malloc(1);
    if(m2 > m1){
        printf("Constrained memory check failed, failed to use alignment space\n");
        while(1);
    }
    uint8_t *m3 = mallocco(0x1000, 1, 0x1000);
    if(!m3 || (uintptr_t)m3 / 0x1000 != ((uint64_t)m3 + 0x1000-1) / 0x1000){
        printf("Constrained memory check failed, failed to avoid boundary %X\n", m3);
        while(1);
    }
    free(m1);
    free(m2);
    free(m3);
    uint8_t *m4 = mallocco(10, 2, 10);
    uint8_t *m5 = mallocco(7, 3, 10);
    uint8_t *m6 = mallocco(10, 3, 10);
    if(!m4 || !m5 || m6){
        printf("Constrained memory check failed, failed to avoid follow constraints\n");
        while(1);
    }
    free(m4);
    free(m5);
    free(m6);
    printf("Passed constrained memory check");
}
void kernel_main(){
    stdioinit();
    stdlib_init();
    testMemory();
    testMemoryConstrained();
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
