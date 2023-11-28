#ifndef SCSI_H_INCLUDED
#define SCSI_H_INCLUDED

#include "stdint.h"

typedef struct{
   union{
      struct{
         uint8_t obsolete : 2;
         uint8_t NACA : 1;
         uint8_t reserved : 3;
         uint8_t vendorSpecific : 2;
      };
      uint8_t bits;
   };
}__attribute__((packed))ScsiControl;

typedef struct{
   uint8_t opCode;
   uint8_t logicalBlockAddressHigh : 5;
   uint8_t misc : 3;
   uint16_t logicalBlockAddressLow;
   union{
      uint8_t transferLength;
      uint8_t parameterListLength;
      uint8_t allocationLength;
   };
   ScsiControl control;
}__attribute__((packed))ScsiCdb6;

typedef struct{
   uint8_t opCode;
   uint8_t serviceAction : 5;
   uint8_t misc1 : 3;
   uint32_t logicalBlockAddress;
   uint8_t misc2;
   union{
      uint16_t transferLength;
      uint16_t parameterListLength;
      uint16_t allocationLength;
   };
   ScsiControl control;
}__attribute__((packed))ScsiCdb10;

typedef struct{
   uint8_t opCode;
   uint8_t serviceAction : 5;
   uint8_t misc1 : 3;
   uint32_t logicalBlockAddress;
   union{
      uint32_t transferLength;
      uint32_t parameterListLength;
      uint32_t allocationLength;
   };
   uint8_t misc2;
   ScsiControl control;
}__attribute__((packed))ScsiCdb12;

typedef struct{
   uint8_t opCode;
   uint8_t misc1;
   uint32_t logicalBlockAddress;
   union{
      uint64_t transferLength;
      uint64_t parameterListLength;
      uint64_t allocationLength;
   };
   uint8_t misc2;
   ScsiControl control;
}__attribute__((packed))ScsiCdb16;

typedef struct{
   uint8_t opCode;
   ScsiControl control;
   uint8_t misc[5];
   uint8_t additionalCdbLength;
   uint16_t serviceAction;
   uint8_t *serviceActionSecificFields;
}__attribute__((packed))ScsiCdbVariable;

typedef enum{
   ScsiCdbSizeVariable = 0,
   ScsiCdbSize6 = 6,
   ScsiCdbSize10 = 10,
   ScsiCdbSize12 = 12,
   ScsiCdbSize16 = 16
}ScsiCdbSize;


typedef struct{
   ScsiCdbSize size;
   union{
      ScsiCdb6 cdb6;
      ScsiCdb6 cdb10;
      ScsiCdb6 cdb12;
      ScsiCdb6 cdb16;
      ScsiCdbVariable cdbVariable;
      uint8_t bytes[16];
   };
}ScsiCDB;

ScsiCDB Scsi_CDB_INQUIRY(uint8_t pageCode, uint16_t allocationLength);
ScsiCDB Scsi_CDB_READ10(uint32_t logicalBlockAddress, uint16_t transferLenght);
ScsiCDB Scsi_CDB_WRITE10(uint32_t logicalBlockAddress, uint16_t transferLenght);

#endif
