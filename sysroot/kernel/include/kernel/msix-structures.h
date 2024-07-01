#ifndef MSIX_STRUCTURES_H_INCLUDED
#define MSIX_STRUCTURES_H_INCLUDED

#include "stdint.h"

typedef struct{
   uint8_t capabilityId;
   uint8_t nextPointer;
   uint16_t tableSize : 11; //N-1 encoded. i.e. tableSize = 0 -> 1 entry.
   uint16_t reserved : 3;
   uint16_t functionMask : 1;
   uint16_t enable : 1;
   uint32_t messageBir : 3;
   uint32_t tableOffsetHigh : 29; //The high 29 bits
   uint32_t pendingBir : 3;
   uint32_t pendingOffsetHigh : 29; //The high 29 bits
}__attribute__((packed))MsiXCapability;

#endif
