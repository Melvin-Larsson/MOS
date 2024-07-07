#include "kernel/paging.h"

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"

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

typedef struct {
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
    uint32_t  physicalAddressHigh : 9;
    uint32_t physicalAddress22To32 : 10;
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

typedef struct {
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

static uint8_t getMaxPhyAddr();
static int isPse36Suported();
static void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

static uint32_t readIA32Efer();
static void writeIa32Efer(uint32_t value);

static PagingConfig32Bit init32BitPaging(PagingConfig32Bit config);
static PagingConfig32Bit clearUnsuported32BitFeatures(PagingConfig32Bit config);
static PagingStatus add32BitPagingEntry(PagingTableEntry entry, uint32_t address);

static volatile uint32_t pageDirectory[1024] __attribute__((aligned(4096)));
static PagingMode pagingMode; 

PagingConfig32Bit paging_init32Bit(PagingConfig32Bit config){
   config = clearUnsuported32BitFeatures(config);
   init32BitPaging(config);
   return config;
}
void paging_start(){
   uint32_t cr0 = readCr0();
   cr0 |= (1 << CR0_PG_POS);
   writeCr0(cr0);
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
        if(address & 0x3FFFFF){
           return PagingUnableToFindEntry;
        }
        if(newEntry.Use4MBPageSize){
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
            return PagingOk;
       }else{
           PageDirectoryEntryTableReference newEntryTablereference = {
               .present = 1,
               .readWrite = (newEntry.readWrite != 0),
               .userSupervisor = (newEntry.userSupervisor != 0),
               .pageWriteThrough = (newEntry.pageWriteThrough != 0),
               .pageCacheDisable = (newEntry.pageCahceDisable != 0),
               .accessed = 0,
               .pageSize = 0,
               .physicalAddress = (newEntry.physicalAddress >> 12)
            };
            pageDirectory[index] = *((uint32_t *)&newEntryTablereference);
            return PagingOk;
         }
    }
   
    PageDirectoryEntryTableReference reference = { .bits = entry };
    uint32_t *subTable = (uint32_t *) reference.physicalAddress;
    uint32_t subTableIndex = (address >> 12) & 0x3FF;
    uint32_t subTableEntry = subTable[subTableIndex];
 
    if(subTableEntry & PAGE_ENTRY_PRESENT){
       return PagingEntryAlreadyPresent;
    }
 
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
    subTable[subTableIndex] = *((uint32_t *)&newEntry4KBPage);
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
