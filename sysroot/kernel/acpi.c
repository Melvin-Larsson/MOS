#include "kernel/acpi.h"
#include "kernel/logging.h"

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
   uint32_t addresses[0];
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


InterruptControllerStructureHeader *getMadtStructure(uint8_t type);
static MADTHeader *findMADT();

static void* findTable(char signature[4]);

static void findRsdp();
static bool isValidRsdp(RSDP *rsdp);

static RSDP rsdp;
static RSDT *rsdt;

void acpi_init(){
   findRsdp();

   int tableEntries = (rsdt->header.length - sizeof(DescriptionTableHeader))/ 4;
   loggDebug("Found %d ACPI tables", tableEntries);

   for(int i = 0; i < tableEntries; i++){
      DescriptionTableHeader *header = (DescriptionTableHeader *)rsdt->addresses[i];
      loggDebug("Found %c%c%c%c", 
         header->signature[0], header->signature[1],
         header->signature[2], header->signature[3]);
   }
}

bool acpi_getIOApicData(IoAcpiData *result){
   InterruptControllerStructureHeader *header = getMadtStructure(TYPE_IO_APIC_STRUCTURE);
   if(header == 0){
      return false;
   }

   IoApic *ioApic = (IoApic *)header;
   *result = (IoAcpiData){
      .id = ioApic->id,
      .address = ioApic->address,
      .globalSystemInterruptBase = ioApic->globalSystemInterruptBase
   };

   return true;
}

bool acpi_getLocalApicData(LocalApicData *result){
   InterruptControllerStructureHeader *header = getMadtStructure(TYPE_LOCAL_APIC_STRUCTURE);
   if(header == 0){
      return false;
   }

   LocalApic *localAcpi = (LocalApic *)header;
   *result = (LocalApicData){
      .acpiProcessorUid = localAcpi->acpiProcessorUid,
      .apicId = localAcpi->apicId,
      .flags = localAcpi->flags
   };

   return true;
}

InterruptControllerStructureHeader *getMadtStructure(uint8_t type){
   MADTHeader *madt = findMADT();
   assert(madt != 0);

   uintptr_t madtAddress = (uintptr_t)madt;
   InterruptControllerStructureHeader *header = 
      (InterruptControllerStructureHeader *)(madtAddress + sizeof(MADTHeader));

   int structureListSize = (madt->header.length - sizeof(MADTHeader));

   while(structureListSize > 0){
      if(header->type == type){
         return header;
      }

      structureListSize -= header->length;
      uintptr_t headerAddress = (uintptr_t)header;
      header = (InterruptControllerStructureHeader* )(headerAddress + header->length);
   }

   return 0;
}



static MADTHeader *findMADT(){
   return findTable("APIC");
}

static void* findTable(char signature[4]){
   int tableEntries = (rsdt->header.length - sizeof(DescriptionTableHeader))/ 4;

   for(int i = 0; i < tableEntries; i++){
      DescriptionTableHeader *header = (DescriptionTableHeader *)rsdt->addresses[i];
      for(int i = 0; i < 4; i++){
         if(header->signature[i] != signature[i]){
            continue;
         }
         return header;
      }
   }
   return 0;
}

static void findRsdp(){
   for(int i = 0xE0000; i < 0xFFFFF; i += 16){
      if(isValidRsdp((RSDP *)i)){
         rsdp = *((RSDP*)i);
         rsdt = (RSDT *)rsdp.rsdtAddress;
         loggInfo("Found rsdp");
         return;
      }
   }

//    TODO: Search EBDA
//    TODO: Search EFI_SYSTEM_TABLE

   loggError("Dit not find rsdp");
}

static bool isValidRsdp(RSDP *rsdp){
   char *expected = "RSD PTR ";

   for(int i = 0; i < 8; i++){
      if(rsdp->signature[i] != expected[i]){
         return false;
      }
   }

   signed char sum = 0;
   signed char *bytes = (signed char *)rsdp;
   for(int i = 0; i < 20; i++){
      sum += bytes[i]; 
   }

   return sum == 0;
}
