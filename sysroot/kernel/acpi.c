#include "kernel/acpi.h"
#include "kernel/logging.h"
#include "kernel/paging.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

#include "stdint.h"
#include "stdbool.h"

#define TYPE_IO_APIC_STRUCTURE 1
#define TYPE_LOCAL_APIC_STRUCTURE 0

typedef struct{
   char signature[8];
   uint8_t checksum;
   char oemId[6];
   uint8_t revision;
   uint32_t rsdtAddress;

   //Availiable in revision 2 and above
   uint32_t length;
   //Availiable in revision 2 and above
   uint64_t xsdtAddress;
   //Availiable in revision 2 and above
   uint8_t extendedChecksum;
   //Availiable in revision 2 and above
   uint8_t reserved[3];
}RSDP;

typedef struct{
   char signature[4];
   uint32_t length;
   uint8_t revision;
   uint8_t checksum;
   char oemId[6];
   char oemTableId[8];
   uint32_t oeamRevision;
   uint32_t creatorId;
   uint32_t creatorRevision;
}DescriptionTableHeader;

typedef struct{
   DescriptionTableHeader header;
   uint32_t addresses[32]; //FIXME: What value?
}RSDT;

typedef struct{
   uint8_t type;
   uint8_t length;
}InterruptControllerStructureHeader;

typedef struct{
   InterruptControllerStructureHeader header;
   uint8_t id;
   uint8_t reserved;
   uint32_t address;
   uint32_t globalSystemInterruptBase;
}IoApic;

typedef struct{
   InterruptControllerStructureHeader header;
   uint8_t acpiProcessorUid;
   uint8_t apicId;
   uint32_t flags;
}LocalApic;

typedef struct{
   DescriptionTableHeader header;
   uint32_t localInterruptControllerAddress;
   uint32_t flags;
}MADTHeader;


uintptr_t getMadtStructure(uint8_t type);
static uintptr_t findMADT();

static uintptr_t findTable(char signature[4]);

static void findRsdp();
static bool isValidRsdp(uintptr_t rsdp);

static RSDP rsdp;
static uintptr_t rsdtAddress;

void acpi_init(){
   findRsdp();

   RSDT rsdt;
   paging_readPhysical(rsdtAddress, &rsdt, sizeof(RSDT));

   int tableEntries = (rsdt.header.length - sizeof(DescriptionTableHeader))/ 4;
   loggDebug("Found %d ACPI tables", tableEntries);

   for(int i = 0; i < tableEntries; i++){
      DescriptionTableHeader header;
      paging_readPhysical(rsdt.addresses[i], &header, sizeof(DescriptionTableHeader));
      loggDebug("Found %c%c%c%c", 
         header.signature[0], header.signature[1],
         header.signature[2], header.signature[3]);
   }
}

bool acpi_getIOApicData(IoAcpiData *result){
   uintptr_t headerAddress = getMadtStructure(TYPE_IO_APIC_STRUCTURE);
   if(headerAddress == 0){
      return false;
   }

   IoApic ioApic;
   paging_readPhysical(headerAddress, &ioApic, sizeof(ioApic));
   *result = (IoAcpiData){
      .id = ioApic.id,
      .address = ioApic.address,
      .globalSystemInterruptBase = ioApic.globalSystemInterruptBase
   };

   return true;
}

bool acpi_getLocalApicData(LocalApicData *result){
   uintptr_t headerAddress = getMadtStructure(TYPE_LOCAL_APIC_STRUCTURE);
   if(headerAddress == 0){
      return false;
   }

   LocalApic localAcpi;
   paging_readPhysical(headerAddress, &localAcpi, sizeof(LocalApic));
   *result = (LocalApicData){
      .acpiProcessorUid = localAcpi.acpiProcessorUid,
      .apicId = localAcpi.apicId,
      .flags = localAcpi.flags
   };

   return true;
}

uintptr_t getMadtStructure(uint8_t type){
   uintptr_t madtAddress = findMADT();
   assert(madtAddress != 0);

   MADTHeader madt;
   paging_readPhysical(madtAddress, &madt, sizeof(MADTHeader));

   uintptr_t headerAddress = madtAddress + sizeof(MADTHeader);

   int structureListSize = (madt.header.length - sizeof(MADTHeader));

   while(structureListSize > 0){
      InterruptControllerStructureHeader header;
      paging_readPhysical(headerAddress, &header, sizeof(InterruptControllerStructureHeader));

      if(header.type == type){
         return headerAddress;
      }

      structureListSize -= header.length;
      headerAddress = (headerAddress + header.length);
   }

   return 0;
}



static uintptr_t findMADT(){
   return findTable("APIC");
}

static uintptr_t findTable(char signature[4]){
   RSDT rsdt;
   paging_readPhysical(rsdtAddress, &rsdt, sizeof(RSDT));
   int tableEntries = (rsdt.header.length - sizeof(DescriptionTableHeader))/ 4;

   for(int i = 0; i < tableEntries; i++){
      DescriptionTableHeader header;
      uintptr_t headerAddress = rsdt.addresses[i];
      paging_readPhysical(headerAddress, &header, sizeof(DescriptionTableHeader));

      for(int j = 0; j < 4; j++){
         if(header.signature[j] != signature[j]){
            continue;
         }
         return headerAddress;
      }
   }
   return 0;
}

static void findRsdp(){
   for(int i = 0xE0000; i < 0xFFFFF; i += 16){
      if(isValidRsdp(i)){
         paging_readPhysical(i, &rsdp, sizeof(RSDP));
         rsdtAddress = rsdp.rsdtAddress;
         loggInfo("Found rsdp");
         return;
      }
   }

//    TODO: Search EBDA
//    TODO: Search EFI_SYSTEM_TABLE

   loggError("Dit not find rsdp");
}

static bool isValidRsdp(uintptr_t rsdpAddress){
   char *expected = "RSD PTR ";

   RSDP rsdp;
   paging_readPhysical(rsdpAddress, &rsdp, sizeof(RSDP));
   for(int i = 0; i < 8; i++){
      if(rsdp.signature[i] != expected[i]){
         return false;
      }
   }

   signed char sum = 0;
   signed char *bytes = (signed char *)&rsdp;
   for(int i = 0; i < 20; i++){
      sum += bytes[i]; 
   }

   return sum == 0;
}
