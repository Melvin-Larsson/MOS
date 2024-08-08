#include "kernel/paging.h"
#include "kernel/physpage.h"
#include "kernel/interrupt.h"
#include "kernel/allocator.h"

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "intmap.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

//Assuming 32 bits

#define CR0_WP_POS 16
#define CR0_PG_POS 31    // Paging enable

#define CR4_PSE_POS  4   // Page Size Extension
#define CR4_PAE_POS  5   // Physical Address Extension
#define CR4_PGE_POS  7   // Page Global Enable
#define CR4_LA57_POS 12  // 5-Level Paging
#define CR4_PCIDE_POS 17 // Process-Context Identifiers
#define CR4_SMEP_POS 20  // Supervisor Mode Execution Protection
#define CR4_SMAP_POS 21  // Supervisor Mode Access Prevention
#define CR4_PKE_POS  22  // Protection Key for User-mode Pages
#define CR4_CET_POS  23  // Control-flow Enforcement Technology
#define CR4_PKS_POS  24  // Protection Key for Supervisor-mode Pages

#define IA32EFER_LME_POS 8
#define IA32EFTER_NXE_POS 11

//CPUID.01H
#define CPUID_EDX_PSE (1 << 3)
#define CPUID_EDX_PAE (1 << 6)
#define CPUID_EDX_PGE (1 << 13)
#define CPUID_EDX_PAT (1 << 16)
#define CPUID_EDX_PSE36 (1 << 17)
#define CPUID_ECX_PCID (1 << 17)

//CPUID.(EAX=07h, ECX=0H)
#define CPUID_EBX_SMEP (1 << 7)
#define CPUID_EBX_SMAP (1 << 20)
#define CPUID_ECX_PKU (1 << 3)
#define CPUID_ECX_OSPKE (1 << 4)
#define CPUID_ECX_CET (1 << 7)
#define CPUID_ECX_LA57 (1 << 16)
#define CPUID_ECX_PKS (1 << 31)

//CPUID.80000001H
#define CPUID_EDX_NX (1 << 20)
#define CPUID_EDX_PAGE1GB (1 << 26)
#define CPUID_EDX_LM (1 << 29)


#define PAGE_ENTRY_PRESENT (1 << 0)
#define PAGE_ENTRY_PAGE_SIZE (1 << 7)

typedef union {
    uint32_t bits;
    struct{
        uint32_t present : 1;
        uint32_t readWrite : 1;
        uint32_t userSupervisor : 1;
        uint32_t pageWriteThrough : 1;
        uint32_t pageCacheDisable : 1;
        uint32_t accessed : 1;
        uint32_t dirty : 1;
        uint32_t pageSize : 1;
        uint32_t global : 1;
        uint32_t ignored1 : 3;
        uint32_t pageAttributeTable : 1;
        uint32_t physicalAddressHigh : 9;
        uint32_t physicalAddress22To32 : 10;
    };
} PageDirectoryEntry32Bit4MB;

typedef union {
   struct{
      uint32_t present : 1;
      uint32_t readWrite : 1;
      uint32_t userSupervisor : 1;
      uint32_t pageWriteThrough : 1;
      uint32_t pageCacheDisable : 1;
      uint32_t accessed : 1;
      uint32_t ignored1 : 1;
      uint32_t pageSize : 1;
      uint32_t ignored2 : 4;
      uint32_t physicalAddress : 20;
   };
   uint32_t bits;
} PageDirectoryEntryTableReference;

typedef union {
    uint32_t bits;
    struct{
        uint32_t present : 1;
        uint32_t readWrite : 1;
        uint32_t userSupervisor : 1;
        uint32_t pageWriteThrough : 1;
        uint32_t pageCacheDisable : 1;
        uint32_t accessed : 1;
        uint32_t dirty : 1;
        uint32_t pageAttributeTable : 1;
        uint32_t global : 1;
        uint32_t ignored1 : 3;
        uint32_t physicalAddress : 20;
    };
} PageTableEntry4KB;

static uint32_t readCr0();
static uint32_t readCr1();
static uint32_t readCr2();
static uint32_t readCr3();
static uint32_t readCr4();

static void writeCr0(uint32_t cr0);
static void writeCr1(uint32_t cr1);
static void writeCr2(uint32_t cr2);
static void writeCr3(uint32_t cr3);
static void writeCr4(uint32_t cr4);

static void memcpyOfSize(void *dst, void *src, int length, AccessSize readSize);
static void memcpyOfSize8(void *dst, void *src, int length);
static void memcpyOfSize16(void *dst, void *src, int length);
static void memcpyOfSize32(void *dst, void *src, int length);
static void memcpyOfSize64(void *dst, void *src, int length);

static uint8_t getMaxPhyAddr();
static PagingMode getPagingMode();
static int isPse36Suported();
static void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

static uint32_t readIA32Efer();
static void writeIa32Efer(uint32_t value);

static PagingConfig32Bit init32BitPaging(PagingConfig32Bit config);
static PagingConfig32Bit clearUnsuported32BitFeatures(PagingConfig32Bit config);
static PagingStatus add32BitPagingEntry(PagingTableEntry entry, uint32_t address);

static void handlePageFault(ExceptionInfo info, void *data);

static volatile uint32_t pageDirectory[1024] __attribute__((aligned(4096)));
static PagingMode pagingMode; 

static Map *physicalToLogicalPage;
static Allocator *pageAllocator;

PagingConfig32Bit paging_init32Bit(PagingConfig32Bit config){
   config = clearUnsuported32BitFeatures(config);
   init32BitPaging(config);
   interrupt_setHandler(handlePageFault, 0, 14);
   physicalToLogicalPage = map_newBinaryMap(intmap_comparitor);
   pageAllocator = allocator_init(0, 1048576);
   return config;
}
void paging_start(){
   uint32_t cr0 = readCr0();
   cr0 |= (1 << CR0_PG_POS);
   writeCr0(cr0);
}
static int getLogicalPage32Bit(uintptr_t *resultPage, unsigned int pageCount4KB){
    AllocatedArea area = allocator_getHinted(pageAllocator, pageCount4KB, AllocatorHintPreferHighAddresses);
    if(area.size == pageCount4KB){
        *resultPage = area.address;
        return area.size;
    }
    return 0;
}

static int getLogicalPage(uintptr_t *resultPage, int pageCount4KB){
    if(pagingMode == PagingMode32Bit){
        return getLogicalPage32Bit(resultPage, pageCount4KB); 
    }

    assert(0);
    return 0;
}

uintptr_t paging_getPhysicalAddress(uintptr_t logical){
    assert(pagingMode == PagingMode32Bit);
    uint32_t directoryIndex = logical >> 22;
    uint32_t entry = pageDirectory[directoryIndex];
    if(!(entry & PAGE_ENTRY_PRESENT)){
        return 0;
    }
    if(entry & PAGE_ENTRY_PAGE_SIZE){
        PageDirectoryEntry32Bit4MB entry4MB = {.bits = entry}; 
        uint32_t offset = logical & 0x3FFFFF;
        return entry4MB.physicalAddressHigh << 32 | entry4MB.physicalAddress22To32 << 22 | offset;
    }

    PageDirectoryEntryTableReference reference = { .bits = entry };
    uint32_t *subTable = (uint32_t *) (reference.physicalAddress < 12);
    uint32_t subTableIndex = (logical >> 12) & 0x3FF;
    PageTableEntry4KB entry4KB = {.bits = subTable[subTableIndex]};
    if(!entry4KB.present){
        return 0;
    }

    uint32_t offset = logical & 0x3FF;
    return entry4KB.physicalAddress << 12 | offset;
}

uintptr_t paging_mapPhysical(uintptr_t address, uint32_t size){
    int physicalPage = address /  (4 * 1024);
    int lastPhysicalPage = (address + size) / (4 * 1024);
    int pageCount = lastPhysicalPage - physicalPage + ((address + size) % (4 * 1024) == 0 ? 0 : 1);

    printf("first %d, last %d\n", physicalPage, lastPhysicalPage);
    printf("page count %d\n", pageCount);

    uintptr_t newPage;
    if(!getLogicalPage(&newPage, pageCount)){
        printf("Not enough memory. What to do? ...What to do?");
        while(1);
    }

    uintptr_t offset = address & 0xFFF;
    uintptr_t resultAddress = newPage << 12 | offset;
    printf("new page %X\n", newPage);
    printf("res %X\n", resultAddress);

    physpage_markPagesAsUsed4KB(physicalPage, pageCount);

    for(int i = 0; i < pageCount; i++){
        PagingTableEntry entry = {
            .physicalAddress = physicalPage << 12,
            .readWrite = 1,
            .pageWriteThrough = 1,
            .pageCahceDisable = 1,
        };
        PagingStatus status = paging_addEntry(entry, newPage << 12);
        assert(status == PagingOk);
        
        intmap_add(physicalToLogicalPage, physicalPage, newPage);

        physicalPage++;
        newPage++;
    }

    return resultAddress;
}
void paging_writePhysical(uintptr_t address, void *data, uint32_t size){
    paging_writePhysicalOfSize(address, data, size, AccessSize8);
}

void paging_writePhysicalOfSize(uintptr_t address, void *data, uint32_t size, AccessSize accessSize){
    uint8_t *dataPtr = data;
    int physicalPage = address / (4 * 1024);
    int lastPhysicalPage = (address + size) / (4 * 1024);
    int pageCount = lastPhysicalPage - physicalPage + ((address + size) % (4 * 1024) != 0 ? 1 : 0);
    int offset = address & 0xFFF;

    for(int i = 0; i < pageCount; i++){
        if(!intmap_contains(physicalToLogicalPage, physicalPage)){
            paging_mapPhysical(physicalPage * 4 * 1024, 4 * 1024);
        }
        assert(intmap_contains(physicalToLogicalPage, physicalPage));
        uintptr_t logicalPage = intmap_get(physicalToLogicalPage, physicalPage);
        uintptr_t pageAddress = logicalPage << 12 | offset;
        uint32_t sizeOnPage = 4 * 1024 - offset;
        if(sizeOnPage > size){
            sizeOnPage = size;
        }
        memcpyOfSize((void*)pageAddress, dataPtr, sizeOnPage, accessSize);
        
        dataPtr += sizeOnPage;
        offset = 0;
        size -= sizeOnPage;
        physicalPage++;
    }
}


void paging_readPhysical(uintptr_t address, void *result, uint32_t size){
    return paging_readPhysicalOfSize(address, result, size, AccessSize8);
}
void paging_readPhysicalOfSize(uintptr_t address, void *result, uint32_t size, AccessSize accessSize){
    uint8_t *resultPtr = result;
    int physicalPage = address / (4 * 1024);
    int lastPhysicalPage = (address + size) / (4 * 1024);
    int pageCount = lastPhysicalPage - physicalPage + ((address + size) % (4 * 1024) != 0 ? 1 : 0);
    int offset = address & 0xFFF;
    for(int i = 0; i < pageCount; i++){
        if(!intmap_contains(physicalToLogicalPage, physicalPage)){
            paging_mapPhysical(physicalPage * 4 * 1024, 4 * 1024);
        }
        assert(intmap_contains(physicalToLogicalPage, physicalPage));
        uintptr_t logicalPage = intmap_get(physicalToLogicalPage, physicalPage);
        uintptr_t pageAddress = logicalPage << 12 | offset;
        
        uint32_t sizeOnPage = 4 * 1024 - offset;
        if(sizeOnPage > size){
            sizeOnPage = size;
        }

        memcpyOfSize(resultPtr, (void*)pageAddress, sizeOnPage, accessSize);

        resultPtr += sizeOnPage;
        offset = 0;
        size -= sizeOnPage;
        physicalPage++;
    }
}

PagingStatus paging_addEntry(PagingTableEntry entry, uintptr_t address){
    if(pagingMode == PagingMode32Bit){
        return add32BitPagingEntry(entry, address);
    }
    printf("Unsuported paging mode. Unable to add entry");
    return PagingUnsuportedOperation;
}

uint32_t lowerBitsMask(int count){
    return 0xFFFFFFFF >> (32 - count);
}

static PagingStatus add32BitPagingEntry(PagingTableEntry newEntry, uint32_t address){
    assert(newEntry.Use4MBPageSize ? (readCr4() & (1 << CR4_PSE_POS)) : 1); 
    assert(newEntry.isGlobal ? (readCr4() & (1 << CR4_PGE_POS)) : 1);
 
    uint16_t index = address >> 22;  
    uint32_t entry = pageDirectory[index]; 
    if(!(entry & PAGE_ENTRY_PRESENT)){
        if(newEntry.Use4MBPageSize){
            if((address & 0x3FFFFF) | (newEntry.physicalAddress & 0x3FFFFF)){
                return PagingUnableToUse4MBEntry;
            }
            uint32_t highAddress = 0;
            if(isPse36Suported()){
                uint8_t M = getMaxPhyAddr();
                if(M > 40){
                    M = 40;
                }
                highAddress = (newEntry.physicalAddress >> 32) & lowerBitsMask(M - 32);
            }
            PageDirectoryEntry32Bit4MB newEntry4MBreference = {
               .present = 1,
               .readWrite = (newEntry.readWrite != 0),
               .userSupervisor = (newEntry.userSupervisor != 0),
               .pageWriteThrough = (newEntry.pageWriteThrough != 0),
               .pageCacheDisable = (newEntry.pageCahceDisable != 0),
               .accessed = 0,
               .dirty = 0,
               .pageSize = 1,
               .global = newEntry.isGlobal,
               .pageAttributeTable = newEntry.pageAttributeTable, //FIXME: chek if suported
               .physicalAddress22To32 = newEntry.physicalAddress >> 22,
               .physicalAddressHigh = highAddress,
            };
            pageDirectory[index] = *((uint32_t *)&newEntry4MBreference);
            allocator_markAsReserved(pageAllocator, address / (4 * 1024), 1024);
            return PagingOk;
       }else{
           printf("new dir\n");
           uintptr_t tablePage = physpage_getPage4KB();
           memset((void*)(tablePage << 12), 0, 4096);
           PageDirectoryEntryTableReference newEntryTablereference = {
               .present = 1,
               .readWrite = (newEntry.readWrite != 0),
               .userSupervisor = (newEntry.userSupervisor != 0),
               .pageWriteThrough = (newEntry.pageWriteThrough != 0),
               .pageCacheDisable = (newEntry.pageCahceDisable != 0),
               .accessed = 0,
               .pageSize = 0,
               .physicalAddress = tablePage,
            };
            pageDirectory[index] = newEntryTablereference.bits;
            entry = pageDirectory[index];
         }
    }

    if(newEntry.Use4MBPageSize){
        return PagingUnableToUse4MBEntry;
    }
    
    PageDirectoryEntryTableReference reference = { .bits = entry };
    uint32_t *subTable = (uint32_t *) (reference.physicalAddress << 12);
    uint32_t subTableIndex = (address >> 12) & 0x3FF;
    uint32_t subTableEntry = subTable[subTableIndex];
 
    if(subTableEntry & PAGE_ENTRY_PRESENT){
       return PagingEntryAlreadyPresent;
    }

    printf("new 4KB page\n");
 
    PageTableEntry4KB newEntry4KBPage = {
       .present = 1,
       .readWrite = (newEntry.readWrite != 0),
       .userSupervisor = (newEntry.userSupervisor != 0),
       .pageWriteThrough = (newEntry.pageWriteThrough != 0),
       .pageCacheDisable = (newEntry.pageCahceDisable != 0),
       .accessed = 0,
       .dirty = 0,
       .pageAttributeTable = newEntry.pageAttributeTable, //FIXME: chek if suported
       .global = newEntry.isGlobal,
       .physicalAddress = (newEntry.physicalAddress >> 12)
    };
    subTable[subTableIndex] = newEntry4KBPage.bits;
    allocator_markAsReserved(pageAllocator, address, 1);
    return PagingOk;
}
 
static PagingConfig32Bit init32BitPaging(PagingConfig32Bit config){
   config = clearUnsuported32BitFeatures(config);

   uint32_t cr0 = readCr0();
   assert((cr0 & (1 << CR0_PG_POS)) == 0);
   cr0 |= (config.writeProtectFromSupervisor != 0) << CR0_WP_POS;
   writeCr0(cr0);

   assert(config.enableControlFlowEnforcment != 0 ? config.writeProtectFromSupervisor != 0 : 1);
   uint32_t cr4 = readCr4();
   cr4 |= (config.use4MBytePages != 0) << CR4_PSE_POS
       | (config.enableGlobalPages != 0) << CR4_PGE_POS
       | (config.fetchProtectFromSupervisor != 0) << CR4_SMEP_POS
       | (config.readProtectFromSupervisor != 0) << CR4_SMAP_POS
       | (config.enableControlFlowEnforcment != 0) << CR4_CET_POS;
   cr4 &= ~(1 << CR4_PAE_POS);
   writeCr4(cr4);

   uint32_t cr3 = readCr3();
   cr3 &= 0xF;
   cr3 |= (uint32_t)pageDirectory;
   writeCr3(cr3);

   return config;
}
static PagingConfig32Bit clearUnsuported32BitFeatures(PagingConfig32Bit config){
   uint32_t eax, ebx, ecx, edx;
   eax = 1;
   cpuid(&eax, &ebx, &ecx, &edx);
   config.use4MBytePages = config.use4MBytePages && (edx & CPUID_EDX_PSE);
   config.enableGlobalPages = config.enableGlobalPages && (edx & CPUID_EDX_PGE);


   eax = 7;
   ecx = 0;
   cpuid(&eax, &ebx, &ecx, &edx);
   config.fetchProtectFromSupervisor = config.fetchProtectFromSupervisor && (ebx & CPUID_EBX_SMEP);
   config.readProtectFromSupervisor = config.readProtectFromSupervisor && (ebx & CPUID_EBX_SMAP);
   config.enableControlFlowEnforcment = config.enableControlFlowEnforcment && (ecx & CPUID_ECX_CET);

   return config;
}

static void handlePageFault(ExceptionInfo info, void *data){
    uint32_t errorCode = info.errorCode;
    assert((errorCode & 1) == 0); //TODO: don't assume this (caused by non-present page)
    assert(getPagingMode() == PagingMode32Bit); //TODO: implement other paging modes also

    uint32_t linearAddress = readCr2();
    printf("Page fault! %X\n", linearAddress);

    uint64_t page = physpage_getPage4MB(); //FIXME: Don't assume there always is 4MB page
    printf("page %d\n", page);
    PagingTableEntry entry = {
        .physicalAddress = page * 4 * 1024 * 1024,
        .readWrite = 1,
        .Use4MBPageSize = 1
    };
    printf("lin %X\n", linearAddress & 0xFFC00000);
    PagingStatus status = paging_addEntry(entry, linearAddress & 0xFFC00000);
    if(status == PagingOk){
        printf("added 4MB entry\n");
        return;
    }
    if(status == PagingUnableToUse4MBEntry){
        physpage_releasePage4MB(page);
        uint64_t page = physpage_getPage4KB();
        PagingTableEntry entry = {
            .physicalAddress = page * 4 * 1024,
            .readWrite = 1,
        };
        PagingStatus status = paging_addEntry(entry, linearAddress & 0xFFFFF000);
        if(status != PagingOk){
            printf("Unable to add 4KB page. Reason %dn", status);
            while(1);
        }
        return;

    }
    printf("Do not know how to handle paging status %d\n", status);
    while(1);
}

static void memcpyOfSize(void *dst, void *src, int length, AccessSize accessSize){
    switch(accessSize){
        case AccessSize8:
            memcpyOfSize8(dst, src, length);
            break;
        case AccessSize16:
            memcpyOfSize16(dst, src, length);
            break;
        case AccessSize32:
            memcpyOfSize32(dst, src, length);
            break;
        case AccessSize64:
            memcpyOfSize64(dst, src, length);
            break;
        default:
            assert(0);
            break;
    }
}
static void memcpyOfSize8(void *dst, void *src, int length){
    uint8_t *src8 = (uint8_t *)src;
    uint8_t *dst8 = (uint8_t *)dst;

    while(length-- > 0){
        *dst8++ = *src8++;
    }
}
static void memcpyOfSize16(void *dst, void *src, int length){
    assert(length % 2 == 0);
    uint16_t *src16 = (uint16_t *)src;
    uint16_t *dst16 = (uint16_t *)dst;

    while(length > 0){
        *dst16++ = *src16++;
        length -= 2;
    }
}
static void memcpyOfSize32(void *dst, void *src, int length){
    assert(length % 4 == 0);
    uint32_t *src32 = (uint32_t *)src;
    uint32_t *dst32 = (uint32_t *)dst;

    while(length > 0){
        *dst32++ = *src32++;
        length -= 4;
    }
}
static void memcpyOfSize64(void *dst, void *src, int length){
    assert(length % 8 == 0);
    uint64_t *src64 = (uint64_t *)src;
    uint64_t *dst64 = (uint64_t *)dst;

    while(length > 0){
        *dst64++ = *src64++;
        length -= 8;
    }
}

static int isPse36Suported(){
    uint32_t eax, ebx, ecx, edx;
    eax = 1;
    cpuid(&eax, &ebx, &ecx, &edx);
    return (edx & CPUID_EDX_PSE36) != 0;
}
static uint8_t getMaxPhyAddr(){
    uint32_t eax, ebx, ecx, edx;
    eax = 0x80000001;
    cpuid(&eax, &ebx, &ecx, &edx);
    if(!(edx & (1 << 29))){ //0x80000008 not suported
        eax = 1;
        cpuid(&eax, &ebx, &ecx, &edx);
        return (edx & CPUID_EDX_PAE) ? 36 : 32;
    }

    eax = 0x80000008;
    cpuid(&eax, &ebx, &ecx, &edx);
    return eax & 0xFF;
}
static PagingMode getPagingMode(){
    uint32_t cr4 = readCr4();

    if((cr4 & (1 << CR4_PAE_POS)) == 0){
        return PagingMode32Bit;
    }
    uint32_t ia32Efer = readIA32Efer();
    if((ia32Efer & (1 << IA32EFTER_NXE_POS)) == 0){
        return PagingModePAE;
    }
    if((cr4 & (1 <<CR4_LA57_POS)) == 0){
        return PagingMode4Level;
    }
    return PagingMode5Level;
}

static void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx){
   __asm__ volatile("cpuid"
         : "+eax"(*eax), "=ebx"(*ebx), "=ecx"(*ecx), "=edx"(*edx)
         : "eax"(*eax)
         );
}
static uint32_t cpuid01eax(){
   uint32_t eax, ebx, ecx, edx;
   __asm__ volatile("cpuid"
         : "+eax"(eax), "=ebx"(ebx), "=ecx"(ecx), "=edx"(edx)
         : "eax"(0x01)
         :
         );
   return edx;
}

static int IA32EferPresent(){
   uint32_t eax, ebx, ecx, edx;
   __asm__ volatile("cpuid"
         : "+eax"(eax), "=ebx"(ebx), "=ecx"(ecx), "=edx"(edx)
         : "eax"(0x80000001)
         :
         );
   return edx & (1 << 20) || edx & (1 << 29);

}

static uint32_t readIA32Efer(){
   assert(IA32EferPresent());

   uint32_t eax, edx;
   __asm__ volatile("rdmsr"
         : "=eax"(eax), "=edx"(edx)
         : "ecx"(0xC0000080)
         );
   return eax;
}

static void writeIa32Efer(uint32_t value){
   __asm__ volatile("wrmsr"
         : 
         :"ecx"(0xC0000080), "eax"(value), "edx"(0)
         :
         );
}


static uint32_t readCr0(){
   uint32_t result;
   __asm__ volatile ("mov %%cr0, %[result]": [result]"=r"(result));
   return result;
}
static void writeCr0(uint32_t cr0){
   __asm__ volatile ("mov %[cr0], %%cr0" : : [cr0]"r"(cr0));
}

static uint32_t readCr1(){
   uint32_t result;
   __asm__ volatile ("mov %%cr1, %[result]": [result]"=r"(result));
   return result;
}
static void writeCr1(uint32_t cr1){
   __asm__ volatile ("mov %[cr1], %%cr1" : : [cr1]"r"(cr1));
}

static uint32_t readCr2(){
   uint32_t result;
   __asm__ volatile ("mov %%cr2, %[result]": [result]"=r"(result));
   return result;
}
static void writeCr2(uint32_t cr2){
   __asm__ volatile ("mov %[cr2], %%cr2" : : [cr2]"r"(cr2));
}

static uint32_t readCr3(){
   uint32_t result;
   __asm__ volatile ("mov %%cr3, %[result]": [result]"=r"(result));
   return result;
}
static void writeCr3(uint32_t cr3){
   __asm__ volatile ("mov %[cr3], %%cr3" : : [cr3]"r"(cr3));
}

static uint32_t readCr4(){
   uint32_t result;
   __asm__ volatile ("mov %%cr4, %[result]": [result]"=r"(result));
   return result;
}
static void writeCr4(uint32_t cr4){
   __asm__ volatile ("mov %[cr4], %%cr4" : : [cr4]"r"(cr4));
}
