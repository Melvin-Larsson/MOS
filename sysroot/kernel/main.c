#include "stdio.h"
#include "stdlib.h"
#include "kernel/interrupt.h"
#include "kernel/apic.h"
#include "kernel/pci.h"
#include "kernel/usb.h"
#include "kernel/keyboard.h"
#include "string.h"
#include "kernel/descriptors.h"
#include "kernel/paging.h"
#include "kernel/physpage.h"
#include "kernel/allocator.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

#include "kernel/usb-mass-storage.h"

char message[] = "Kernel started!\n";
static void printPciDevices(PciDescriptor *descriptors, int count){
    return;
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
static PciDescriptor* getXhcdDevice(PciDescriptor* descriptors, int count){
    for(int i = 0; i < count; i++){
        PciHeader *header = &(descriptors[i].pciHeader);

        if(header->classCode == PCI_CLASS_SERIAL_BUS_CONTROLLER
            && header->subclass == PCI_SUBCLASS_USB_CONTROLLER
            && header->progIf == PCI_PROG_IF_XHCI ){

            return &(descriptors[i]);
        }
    }
    return 0;
}
static void initXhci(PciDescriptor pci){
    Usb usb;
    if(usb_init(pci, &usb) != StatusSuccess){
        printf("Failed to initialize USB\n");
        return;
    }
    while(1){
        UsbDevice device;
        printf("Wait for attach\n");
        while(usb_getNewlyAttachedDevices(&usb, &device, 1) == 0);
        printf("device attach\n");
//         UsbMassStorageDevice res;
//         UsbMassStorageStatus status = usbMassStorage_init(&device, &res);
//         if(status == UsbMassStorageSuccess){
//             printf("Found deice!\n");
//             uint32_t buffer[64];
//             usbMassStorage_read(&res, 0, buffer, sizeof(buffer));
//             printf("buffer: ");
//             for(int i = 0; i < sizeof(buffer); i++){
//                 printf("%X ", buffer[i]);
//             }
//             printf("\n");
//             while(1);
//         }else{
//             printf("Failed to init: %X\n", status);
//             while(1);
//         }
        KeyboardStatus status = keyboard_init(&device);
        char buffer[100];
        keyboard_getStatusCode(status, buffer);
        printf("%s\n", buffer);
    }
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
void assert_little_endian(){
    uint32_t i = 0x12345678;
    uint8_t *ptr = (uint8_t*)&i;
    assert(ptr[0] == 0x78);
    assert(ptr[1] == 0x56);
    assert(ptr[2] == 0x34);
    assert(ptr[3] == 0x12);
}

void kernel_main(){
    while(1);
    stdioinit();
    stdlib_init();
//     testMemory();
//     testMemoryConstrained();
    printf(message);
    interruptDescriptorTableInit(); 
    assert_little_endian();

    physpage_init();


//     uintptr_t myAddress = 100 * 4 * 1024 * 1024;
//     volatile uint32_t *myPtr = (volatile uint32_t *)(myAddress);
//     *myPtr = 0x69;

    PagingConfig32Bit config = {
        .use4MBytePages = 1,
    };
    PagingConfig32Bit newConfig = paging_init32Bit(config);
    assert(config.use4MBytePages == newConfig.use4MBytePages);

    for(uint32_t i = 0; i < 10; i++){
        PagingTableEntry entry = {
            .physicalAddress = i * 4 * 1024 * 1024,
            .readWrite = 1,
            .pageWriteThrough = 1,
            .pageCahceDisable = 1,
            .Use4MBPageSize = 1
        };
        
        physpage_markPagesAsUsed4MB(i, 1);


        PagingStatus status = paging_addEntry(entry, i * 4 * 1024 * 1024);
        assert(status == PagingOk);
    }

    

    paging_start();

//     const uintptr_t ba = 0xFEBB0000;
//     PagingTableEntry entry = {
//         .physicalAddress = ba,
//         .readWrite = 1,
//         .pageWriteThrough = 1,
//         .pageCahceDisable = 1,
//         .Use4MBPageSize = 0
//     };

//     uintptr_t add = 0xFFFFE << 12;
//     PagingStatus status = paging_addEntry(entry, add);
//     assert(status == PagingOk);

    //0xFFFFE << 12

//     uintptr_t address = paging_mapPhysical(0xFEBB0000, 4096) + 4;
//     printf("address here %X\n", address);

//     volatile uint32_t *ptr = (volatile uint32_t *)(address);
    uint32_t result = 0;
    paging_readPhysical(0xFEBB0004, &result, 4);
    printf("ptr %X\n", result);
    while(1);

//     PagingTableEntry entry = {
//         .physicalAddress = myAddress,
//         .readWrite = 1,
//         .pageWriteThrough = 1,
//         .pageCahceDisable = 1,
//         .Use4MBPageSize = 0
//     };

//     uintptr_t myLogicalAddress = 20 * 4 * 1024 * 1024;
//     PagingStatus status = paging_addEntry(entry, myLogicalAddress);
//     assert(status == PagingOk);
        

// //     uintptr_t myLogicalAddress = paging_mapPhysical(myAddress, 10);
//     volatile uint32_t *myLogicalPtr = (volatile uint32_t *)myLogicalAddress;


//     printf("value %X\n", *myLogicalPtr);

//     while(1);

//     printf("APIC present: %b\n", apic_isPresent());

    PciDescriptor devices[20];
    int count = pci_getDevices(devices, 10);
    printPciDevices(devices, count);
    PciDescriptor *xhcDevice = getXhcdDevice(devices, count);
    if(!xhcDevice){
        printf("Error: Could not find xhc device!");
    }else{
        printf("Found a xhc device!\n");
        initXhci(*xhcDevice);
    }

    printf("end\n");
    while(1);
}
