#include "kernel/scsi.h"
#include "stdlib.h"

#define OPCODE_INQUIRY 0x12
#define OPCODE_READ10 0x28
#define OPCODE_WRITE10 0x2A
#define OPCODE_TEST_UNIT_READY 0
#define OPCODE_READ_CAPACITY 0x25

typedef struct{
   uint8_t opCode;
   uint8_t evpd : 1;
   uint8_t reserved : 7;
   uint8_t pageCode;
   uint16_t allocationLength;
   ScsiControl control;
}__attribute__((packed))Inquiry;

typedef struct{
   uint8_t opCode;
   uint8_t obsolete : 2;
   uint8_t rarc : 1;
   uint8_t fua : 1;
   uint8_t dpo : 1;
   uint8_t rwprotect : 3;
   uint32_t logicalBlockAddress;
   uint8_t groupNumber : 5;
   uint8_t reserved2 : 3;
   uint16_t transferLength;
   ScsiControl control;
}__attribute__((packed))ReadWrite10;

ScsiCDB Scsi_CDB_INQUIRY(uint8_t pageCode, uint16_t allocationLength){
   uint8_t evpd = 0;
   if(pageCode != 0){
      evpd = 1;
   }
   Inquiry cdb6  = (Inquiry){
      .opCode = OPCODE_INQUIRY,
      .evpd = evpd,
      .pageCode = pageCode,
      .allocationLength = allocationLength,
      .control.bits = 0,

      .reserved = 0,

   };
   ScsiCDB cdb = (ScsiCDB){
      .size = ScsiCdbSize6,
   };
   memcpy(&cdb.cdb6, &cdb6, sizeof(ScsiCdb6));
   return cdb;
}

ScsiCDB Scsi_CDB_READ10(uint32_t logicalBlockAddress, uint16_t transferLenght){
   ReadWrite10 read10 = (ReadWrite10){
      .opCode = OPCODE_READ10,
      .obsolete = 0,
      .rwprotect = 0, //FIXME: protection info
      .dpo = 0,
      .fua = 0,
      .rarc = 0,
      .logicalBlockAddress = __builtin_bswap32(logicalBlockAddress),
      .groupNumber = 0,
      .transferLength = __builtin_bswap16(transferLenght),
      .control.bits = 0,

      .reserved2 = 0,
   };
   ScsiCDB cdb = (ScsiCDB){
      .size = ScsiCdbSize10
   };
   memcpy(&cdb.cdb10, &read10, sizeof(ScsiCdb10));
   return cdb;
}

ScsiCDB Scsi_CDB_WRITE10(uint32_t logicalBlockAddress, uint16_t transferLenght){
   ReadWrite10 read10 = (ReadWrite10){
      .opCode = OPCODE_WRITE10,
      .obsolete = 0,
      .rwprotect = 0, //FIXME: protection info
      .dpo = 0,
      .fua = 0,
      .logicalBlockAddress = __builtin_bswap32(logicalBlockAddress),
      .groupNumber = 0,
      .transferLength = __builtin_bswap16(transferLenght),
      .control.bits = 0,

      .rarc = 0,
      .reserved2 = 0,
   };
   ScsiCDB cdb = (ScsiCDB){
      .size = ScsiCdbSize10
   };
   memcpy(&cdb.cdb10, &read10, sizeof(ScsiCdb10));
   return cdb;
}
ScsiCDB Scsi_CDB_TestUnitReady(){
   ScsiCDB cdb = (ScsiCDB) {
      .size = ScsiCdbSize6,
   };
   memset(&cdb.cdb6, 0, sizeof(ScsiCdbSize6));
   cdb.cdb6.opCode = OPCODE_TEST_UNIT_READY;
   return cdb;
}
ScsiCDB Scsi_CDB_ReadCapacity(){
   ScsiCDB cdb = (ScsiCDB){
      .size = ScsiCdbSize10,
   };
   memset(&cdb.cdb10, 0, sizeof(ScsiCdb10));
   cdb.cdb10.opCode = OPCODE_READ_CAPACITY;
   return cdb;
}

