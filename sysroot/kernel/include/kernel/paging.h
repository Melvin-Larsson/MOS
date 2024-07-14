#ifndef PAGING_H_INDLUDED
#define PAGING_H_INDLUDED

#include "stdint.h"

typedef enum{
   PagingOk,
   PagingUnableToFindEntry,
   PagingUnsuportedOperation,
   PagingEntryAlreadyPresent,
   PagingUnableToUse4MBEntry,
}PagingStatus;

typedef enum{
   PagingMode32Bit,
   PagingModePAE,
   PagingMode4Level,
   PagingMode5Level
}PagingMode;


typedef struct{
   int writeProtectFromSupervisor; //CR0.WP Disallow supervisor to write to read only addresses.
   int use4MBytePages; //CR4.PSE
   int enableGlobalPages; //CR4.PGE
   int fetchProtectFromSupervisor; //CR4.SMEP Disallow supervisor from fetching ?
   int readProtectFromSupervisor; //CR4.SMAP
   int enableControlFlowEnforcment; //CR4.CET, only allowed if writeProtectFromSupervisor is set
}PagingConfig32Bit;

typedef struct{
   uint64_t physicalAddress;
   int readWrite;
   int userSupervisor;
   int pageWriteThrough;
   int pageCahceDisable;
   int Use4MBPageSize;
   int isGlobal;
   int pageAttributeTable;
}PagingTableEntry;

PagingConfig32Bit paging_init32Bit(PagingConfig32Bit config);
void paging_start();
PagingStatus paging_addEntry(PagingTableEntry entry, uintptr_t address);

#endif
