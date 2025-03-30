#include "kernel/kernel-io.h"
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
#include "kernel/serial.h"
#include "kernel/pit.h"
#include "kernel/acpi.h"
#include "kernel/ioapic.h"
#include "kernel/threads.h"
#include "kernel/timer.h"
#include "kernel/memory.h"
#include "kernel/mass-storage.h"
#include "kernel/fat.h"
#include "kernel/ps2-8042.h"

#include "kernel/task.h"

#include "kernel/logging.h"

#include "stdio.h"

#include "utils/assert.h"

#include "kernel/usb-mass-storage.h"

static FileSystem fs;

extern uint32_t __bss_start;
extern uint32_t __bss_end;

static void printPciDevices(PciDescriptor *descriptors, int count){
    return;
    loggInfo("%d Devices detected:", count);
    for(int i = 0; i < count; i++){
        PciHeader *header = &(descriptors[i].pciHeader);
        char className[50];
        char subclassName[50];
        char progIfName[50];
        
        pci_getClassName(header, className);
        pci_getSubclassName(header, subclassName);
        pci_getProgIfName(header, progIfName);
        loggInfo("%d: %s(%X) - %s(%X) - %s(%X)", i,
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
    Usb *usb = kmalloc(sizeof(Usb));
    UsbDevice *device = kmalloc(sizeof(UsbDevice));
    UsbMassStorageDevice *res = kmalloc(sizeof(UsbMassStorageDevice));
//     MassStorageDevice *ms = kmalloc(sizeof(MassStorageDevice));

    if(usb_init(pci, usb) != StatusSuccess){
        loggError("Failed to initialize USB");
        while(1);
        return;
    }
    while(1){
        loggDebug("Wait for attach");
        while(usb_getNewlyAttachedDevices(usb, device, 1) == 0);
        loggInfo("Device attached");
         UsbMassStorageStatus status = usbMassStorage_init(device, res);
         if(status == UsbMassStorageSuccess){
             uint8_t buffer[1024];
             usbMassStorage_read(res, 0, buffer, sizeof(buffer));
             for(size_t i = 0; i < sizeof(buffer); i++){
                 printf("%c", buffer[i]);
             }
             while(1);
         }else{
             loggWarning("Failed\n");
         }
//             loggInfo("Found device!\n");
//             massStorageDevice_initUsb(res, ms);

//             if(fat_init(ms, &fs) == FatStatusFailure){
//                 loggError("Failed to init fat");
//                 while(1);
//             }
//             return;
//             loggDebug("Init fat\n");

// //             char buff[20];
// //             for(int i = 0; i < 1; i++){
// //                 loggDebug("Create %d", i);
// //                 sprintf(buff, "/f_%d.txt", i);
// //                 File *f = fs.createFile(&fs, buff);
// //                 if(!f){
// //                     f = fs.openFile(&fs, buff);
// //                 }
// //                 fs.writeFile(f, "Hello From Os!", 14);
// //                 loggDebug("Closing file...");
// //                 fs.closeFile(f);
// //             }

//             Directory *dir= fs.openDirectory(&fs, "/");
//             loggDebug("Open dir %X\n", dir);

//             while(true){
//                 DirectoryEntry *entry = fs.readDirectory(dir);
//                 if(!entry){
//                     break;
//                 }
//                 loggInfo("Read %s", entry->filename);
//             }

//             loggInfo("Read all files");
//             fs.closeDirectory(dir);
//             loggInfo("Directory closed");
//             return;
//             fs.closeFileSystem(&fs);
//             loggInfo("Fs closed");

//             uint8_t buffer[512];
//             usbMassStorage_read(&res, 0, buffer, sizeof(buffer));
//             char str[1024];
//             str[0] = 0;
//             char strBuff[64];
//             loggInfo("buffer: ");
//             for(int i = 0; i < sizeof(buffer); i++){
//                 sprintf(strBuff, "%X", buffer[i]);
//                 strAppend(str, strBuff);
//             }
//             loggInfo("%s", str);
//             while(1);
//         }else{
//             printf("Failed to init: %X\n", status);
//         }
/*        KeyboardStatus status = keyboard_init(&device);
        char buffer[100];
        keyboard_getStatusCode(status, buffer);
        loggInfo("Keyboard status code: %s", buffer);*/
    }
}
static int overlaps(uint8_t *p1, int s1, uint8_t *p2, int s2){
    return p1 + s1 > p2 && p1 < p2 + s2;

}
static void testMemory(){
    uint8_t *m1 = kmalloc(10);
    uint8_t *m2 = kmalloc(10);
    uint8_t *m3 = kmalloc(10);
    if(overlaps(m1, 10, m2, 10) || overlaps(m2, 10, m3, 10) || overlaps(m1, 10, m3, 10)){
        kprintf("Memory check failed, memories overlap!\n");
        while(1);
    }
    kfree(m2);
    uint8_t *m4 = kmalloc(10);
    if(m2 != m4){
        kprintf("Memory check failed, failed to reuse memory(1)!\n");
        while(1);
    }
    kfree(m4);
    uint8_t *m5 = kmalloc(1);
    uint8_t *m6 = kmalloc(1);
    if(!overlaps(m4, 10, m5, 1) || !overlaps(m4, 10, m6, 1)){
        kprintf("Memory check failed, failed to reuse memory(2)!\n");
        while(1);
    }
    kfree(m1);
    kfree(m3);
    kfree(m5);
    kfree(m6);
    uint8_t *m7 = kmalloc(10);
    if(m7 != m1){
        kprintf("Memory check failed, failed to reuse memory(3)!\n");
        while(1);
    }
    kfree(m7);
    kprintf("Passed memory check!\n");
}

static void testMemoryConstrained(){
    uint8_t *m1 = kmallocco(10, 0x100, 0);
    if((uintptr_t)m1 % 0x100 != 0){
        kprintf("Constrained memory check failed, failed to align!\n");
        while(1);
    }
    uint8_t *m2 = kmalloc(1);
    if(m2 > m1){
        kprintf("Constrained memory check failed, failed to use alignment space\n");
        while(1);
    }
    uint8_t *m3 = kmallocco(0x1000, 1, 0x1000);
    if(!m3 || (uintptr_t)m3 / 0x1000 != ((uintptr_t)m3 + 0x1000-1) / 0x1000){
        kprintf("Constrained memory check failed, failed to avoid boundary %X\n", m3);
        while(1);
    }
    kfree(m1);
    kfree(m2);
    kfree(m3);
    uint8_t *m4 = kmallocco(10, 2, 10);
    uint8_t *m5 = kmallocco(7, 3, 10);
    uint8_t *m6 = kmallocco(10, 3, 10);
    if(!m4 || !m5 || m6){
        kprintf("Constrained memory check failed, failed to avoid follow constraints\n");
        while(1);
    }
    kfree(m4);
    kfree(m5);
    kfree(m6);
    kprintf("Passed constrained memory check");
}
void assert_little_endian(){
    uint32_t i = 0x12345678;
    uint8_t *ptr = (uint8_t*)&i;
    assert(ptr[0] == 0x78);
    assert(ptr[1] == 0x56);
    assert(ptr[2] == 0x34);
    assert(ptr[3] == 0x12);
}

void serial_writer(const char *str){
    serial_write(COM1, str);
    serial_write(COM1, "\n\r");
}
void console_writer(LoggContext context, LoggLevel level, const char *format, va_list args){
    (void)context;

    KIOColor prevColor = kio_getColor();
    KIOColor newColor;
    switch(level){
        case LoggLevelDebug:
            newColor = KIOColorGray;
            break;
        case LoggLevelInfo:
            newColor = KIOColorWhite;
            break;
        case LoggLevelWarning:
            newColor = KIOColorYellow;
            break;
        case LoggLevelError:
            newColor = KIOColorRed;
            break;
        default:
            newColor = prevColor;
            break;
    }
    kio_setColor(newColor);
    vkprintf(format, args);
    kprintf("\n");
    kio_setColor(prevColor);
}

void myUserspaceFunc(){
    char *hello = "Hello World! (1)\n";
    volatile uint32_t eax = (2 << 16 | 1);
    volatile uint32_t bex = (uint32_t)hello;
    __asm__ volatile("int $0x80"
            : 
            : "a"(eax), "b"(bex));

    while(1);
}

void myUserspaceFunc2(){
    char *hello = "Hello World! (2)\n";
    volatile uint32_t eax = (2 << 16 | 1);
    volatile uint32_t bex = (uint32_t)hello;
    __asm__ volatile("int $0x80"
            : 
            : "a"(eax), "b"(bex));
    while(1);
}

static Semaphore *s1;
static Semaphore *s2;

void t1(){
//     kprintf("Hello world! (1)\n");
//     semaphore_aquire(s1);
//     while(1);
    while(1){
        kprintf("1");
        semaphore_release(s2);
        thread_sleep(1000);
    }
}
void t2(){
//     kprintf("Hello world! (2)\n");
//     semaphore_aquire(s2);
//     while(1);
    while(1){
        semaphore_aquire(s2);
        kprintf("2");
        semaphore_aquire(s1);
    }
}

void initLogging(){
    logging_init();

    SerialPortConfig serialConfig = serial_defaultConfig();
    serial_initPort(COM1, serialConfig);
    LoggWriter serialWriter = logging_getDefaultWriter(serial_writer);
    serialWriter.loggLevel = LoggLevelDebug;
    logging_addWriter(serialWriter);

    LoggWriter consoleWriter = logging_getCustomWriter(console_writer);
    consoleWriter.loggLevel = LoggLevelInfo,

    logging_addWriter(consoleWriter);
}

/*
static void enter_usermode(){
    loggInfo("Enter user");
    __asm__ volatile("\
        mov $(4 << 3 | 3), %%eax; \
        mov %%ax, %%ds;    \
        mov %%ax, %%es;    \
        mov %%ax, %%fs;    \
        mov %%ax, %%gs;    \
                           \
        mov %%esp, %%eax;  \
        push $(4 << 3 | 3); \
        push $0xA00000; \
        pushf; \
        push $(3 << 3 | 3); \
        push $0x800000; \
        iret;  "
        :
        :
        : "eax");
}
*/

PagingContext *userspaceContext;
PagingContext *kernelContext;

static uint64_t timeMillis = 0;
void timerHandler(void *data){
    (void)data;

    timeMillis += 10;
    uint32_t minute = timeMillis / (60 * 1000);
    uint32_t seconds = ((timeMillis - minute * 60 * 1000) / 1000);
    uint32_t millis = ((timeMillis - minute * 60 * 1000 - seconds * 1000));
    kclear();
    kprintf("%d:%d:%d", minute, seconds, millis);
}

char ps2_scancode_to_ascii_set_2(uint8_t scancode){
    char scancode_set2[256] = {
        [0x1C] = 'a', [0x32] = 'b', [0x21] = 'c', [0x23] = 'd',
        [0x24] = 'e', [0x2B] = 'f', [0x34] = 'g', [0x33] = 'h',
        [0x43] = 'i', [0x3B] = 'j', [0x42] = 'k', [0x4B] = 'l',
        [0x3A] = 'm', [0x31] = 'n', [0x44] = 'o', [0x4D] = 'p',
        [0x15] = 'q', [0x2D] = 'r', [0x1B] = 's', [0x2C] = 't',
        [0x3C] = 'u', [0x2A] = 'v', [0x1D] = 'w', [0x22] = 'x',
        [0x35] = 'y', [0x1A] = 'z',
        
        [0x29] = ' ',
        [0x66] = '\b',
        [0x5A] = '\n'
    };

    return scancode_set2[scancode];
}

char ps2_scancode_to_ascii(uint8_t scancode) {
    static const char scancode_set2[256] = {
        [0x1E] = 'a', [0x30] = 'b', [0x2E] = 'c',
        [0x20] = 'd', [0x12] = 'e', [0x21] = 'f',
        [0x22] = 'g', [0x23] = 'h', [0x17] = 'i',
        [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
        [0x32] = 'm', [0x31] = 'n', [0x18] = 'o',
        [0x19] = 'p', [0x10] = 'q', [0x13] = 'r',
        [0x1F] = 's', [0x14] = 't', [0x16] = 'u',
        [0x2F] = 'v', [0x11] = 'w', [0x2D] = 'x',
        [0x15] = 'y', [0x2C] = 'z', [0x29] = ' ',
        [0x1C] = '\n', // Enter
        [0x0E] = '\b', // Backspace
        [0x39] = ' ',
    };
    return scancode_set2[scancode] ? scancode_set2[scancode] : 0; // '?' for unknown codes
}

bool prefix(char *c1, char *c2){
    while(*c1 && *c2){
        if(*c1++ != *c2++){
            return false;
        }
    }

    return true;
}

bool cmp(char *c1, char *c2){
    while(*c1 && *c2){
        if(*c1++ != *c2++){
            return false;
        }
    }
    return *c1 == *c2;
}

static char buffer[128];
static int characters = 0;
static char currDir[128] = "/";
static volatile char newChar = 0;

static int get_path(char *result, char *str){
    char *start = str;
    char *end = start;
    while(*end && *end != ' '){
        end++;
    }
    char filename[32];
    int length = end - start;
    memcpy(filename, start, length);
    filename[length] = 0;

    sprintf(result, "%s%s", currDir, filename);

    return length;
}

void file_system(){
    kclear();
    while(1){
        while(newChar == 0);
        char c = newChar;
        newChar = 0;
        if(c == '\n'){
            if(cmp(buffer, "ls")){
                Directory *dir = fs.openDirectory(&fs, "/");
                kprintf("\n");
                while(1){
                    DirectoryEntry *dirEntry = fs.readDirectory(dir);
                    if(!dirEntry){
                        break;
                    }
                    kprintf("%s ", dirEntry->filename);
                    kfree(dirEntry->filename);
                    kfree(dirEntry->path);
                    kfree(dirEntry);
                }
                fs.closeDirectory(dir);
                
            }
            else if(prefix("touch ", buffer)){
                char *start = buffer + strlen("touch ");
                while(1){
                    if(*start == 0){
                        break;
                    }

                    char path[128];
                    start += get_path(path, start);
                    File *file = fs.createFile(&fs, path);
                    if(file){
                        fs.closeFile(file);
                    }
                }
                characters = 0;
                buffer[0] = 0;
            }
            else if(prefix("edit ", buffer)){
                char *start = buffer + strlen("edit ");
                char path[128];
                start += get_path(path, start) + 1;
                File *f = fs.openFile(&fs, path);
                if(f){
                    fs.writeFile(f, start, strlen(start));
                    fs.closeFile(f);
                }
                characters = 0;
                buffer[0] = 0;
            }
            else if(prefix("cat ", buffer)){
                char *start = buffer + strlen("cat ");
                char path[128];
                start += get_path(path, start);
                File *f = fs.openFile(&fs, path);
                if(f){
                    char res[512];
                    int length = fs.readFile(f, res, sizeof(res));
                    fs.closeFile(f);
                    res[length] = 0;
                    kprintf("\n%s\n", res);
                }
                characters = 0;
                buffer[0] = 0;
            }
            else{
                kprintf("\nUnknown command '%s'", buffer);
            }
            buffer[0] = 0;
            characters = 0;
            kprintf("\n");
        }
        else if (c == '\b'){
            if(characters > 0){
                kprintf("\b");
                buffer[characters] = 0;
                characters --;
            }
        }
        else{
            kprintf("%c", c);
            buffer[characters++] = c;
            buffer[characters] = 0;
        }



    }
}

void key_handler(uint8_t scancode){
    static bool ignore_next = false;

    if(scancode == 0xF0){
        ignore_next = true;
    }
    else if(ignore_next){
        ignore_next = false;
    }
    else{
        char c = ps2_scancode_to_ascii_set_2(scancode);
        newChar = c;
    }
}

static bool writeWithResend(Ps28042PortId portId, uint8_t command1){
    for(int i = 0; i < 3; i++){
        loggDebug("write");
        ps2_8042_writeToPort(portId, command1);
        uint8_t res;
        ps2_8042_readPortBlocking(portId, &res);
        loggDebug("read %X", res);
        if(res == 0xFA){
            return true;
        }
    }

    return false;
}

void kernel_main(){
    memset(&__bss_start, 0, &__bss_end - &__bss_start);
    timeMillis = 0;
    kio_init();
    memory_init();

    testMemory();
    testMemoryConstrained();
    interruptDescriptorTableInit(); 
    kprintf("Kernel started\n");
    assert_little_endian();
    initLogging();

    physpage_init();
    physpage_markPagesAsUsed4MB(0, 1);
    physpage_markPagesAsUsed4KB(4194304, 4194304);
    paging_init();

    acpi_init();
    ioapic_init();

//     assert(apic_isPresent());
//     apic_enable();

    timers_init();

    initKernelTask(4 * 1024 * 1024);

    uintptr_t userspaceAddress = 0x800000;
    uintptr_t func2Addr = (myUserspaceFunc2 - myUserspaceFunc) + userspaceAddress;
    assert(func2Addr > userspaceAddress);

    physpage_markPagesAsUsed4MB(2, 1);

    uintptr_t funcAddr = (uintptr_t)myUserspaceFunc;
    memcpy((void*)userspaceAddress, (void*)funcAddr, 4096);

    PagingConfig32Bit config = {
        .use4MBytePages = 1,
    };
    kernelContext = paging_create32BitContext(config);
    assert(config.use4MBytePages == kernelContext->config32Bit.use4MBytePages);
    PagingTableEntry entry = {
        .physicalAddress = 0,
        .readWrite = 1,
        .pageWriteThrough = 1,
        .pageCahceDisable = 1,
        .Use4MBPageSize = 1,
    };
    PagingStatus status = paging_addEntryToContext(kernelContext, entry, 0);
    assert(status == PagingOk);
    loggDebug("Status %X", status);

    paging_setContext(kernelContext);
    paging_start();

    userspaceContext = paging_create32BitContext(config);
    PagingTableEntry userSpaceEntry = {
        .physicalAddress = userspaceAddress,
        .readWrite = 1,
        .pageWriteThrough = 1,
        .pageCahceDisable = 1,
        .Use4MBPageSize = 1,
        .userSupervisor = 1
    };
    uintptr_t newAddress = 0x800000;
    uint32_t status1 = paging_addEntryToContext(userspaceContext, entry, 0);
    uint32_t status2 = paging_addEntryToContext(userspaceContext, userSpaceEntry, newAddress);

    loggDebug("status %X %X\n", status1, status2);

    paging_stop();
    paging_setContext(userspaceContext);
    paging_start();

    kprintf("jump\n");

    uint32_t eflags = 0;
    __asm__ volatile("\
            pushf;\
            pop %[reg]"
            : [reg]"=r"(eflags));

    threads_init();
//     ThreadConfig thread1 = {
//         .start = (void (*)(void*))t1,
//         .data = 0,
//         .cs = (1 << 3) | 0,
//         .ss = (2 << 3) | 0,
//         .esp = 0xB00000,
//         .eflags = eflags
//     };
//     ThreadConfig thread2 = {
//         .start = (void (*)(void*))t2,
//         .data = 0,
//         .cs = (1 << 3) | 0,
//         .ss = (2 << 3) | 0,
//         .esp = 0xA00000,
//         .eflags = eflags
//     };
//     s1 = semaphore_new(0);
    s2 = semaphore_new(0);

//     thread_start(thread1);
//     thread_start(thread2);

    PciDescriptor devices[20];
    int count = pci_getDevices(devices, 10);
    loggInfo("here\n");
    printPciDevices(devices, count);
    PciDescriptor *xhcDevice = getXhcdDevice(devices, count);
    if(!xhcDevice){
        loggError("Error: Could not find xhc device!");
    }else{
        loggInfo("Found a xhc device!");
        initXhci(*xhcDevice);
    }

    Ps28042Status ps2Status = ps2_8042_init();
    if(ps2Status == Ps28042StatusSucess){
        loggDebug("Initialized ps2 8042");
    }else{
        loggError("Failed to initialize ps2 8042, reason %d", ps2Status);
    }

    Ps28042Port port = ps2_8042_getPortInfo(Port1);
    if(port.deviceType != 0x83AB && port.deviceType != 0xC1AB){
        loggDebug("No keyboard on port 1");
        port = ps2_8042_getPortInfo(Port2);

        if(port.deviceType != 0x83AB && port.deviceType != 0xC1AB){
            loggDebug("No keyboard on port 2");
            while(1);
        }
    }

    uint8_t trash;
    loggDebug("Clear buffer");
    for(int i = 0; i < 100; i++){
        ps2_8042_readPort(port.portId, &trash);
    }

    loggDebug("3: %d", writeWithResend(port.portId, 0xF0));
    loggInfo("4: %d", writeWithResend(port.portId, 2));

    loggDebug("scan");
    loggDebug("2:%d", writeWithResend(port.portId, 0xF4));

    ps2_8042_set_interrupt_handler(port.portId, key_handler);

    loggInfo("end");

    file_system();

    while(1);
}
