#ifndef XHCD_RING_H_INCLUDED
#define XHCD_RING_H_INCLUDED

#include "stdint.h"
#include "xhcd-registers.h"
#include "xhcd-hardware.h"

enum TransferType{
   NoDataStage = 0,
   OutDataStage = 2,
   InDataStage = 3
};

typedef struct{
   uint64_t ringSegment;
   uint32_t reserved : 22;
   uint32_t interrupterTarget : 10;
   uint32_t cycleBit : 1;
   uint32_t toggleCycle : 1;
   uint32_t reserved2 : 2;
   uint32_t chainBit : 1;
   uint32_t interruptOnCompletion : 1;
   uint32_t reserved3 : 4;
   uint32_t trbType : 6;
   uint32_t reserved4 : 16;
}__attribute__((packed))LinkTRB;

typedef struct{
   union{
      struct{
         uint32_t r0;
         uint32_t r1;
         uint32_t r2;
         uint32_t r3; };
      struct{
         uint64_t dataBufferPointer;
         uint32_t transferLength : 17;
         uint32_t size : 5;
         uint32_t interrupterTarget : 10;
         uint32_t cycleBit : 1;
         uint32_t evaluateNextTrb : 1;
         uint32_t interruptOnShortPacket : 1;
         uint32_t noSnoop : 1;
         uint32_t chainBit : 1;
         uint32_t interruptOnCompletion : 1;
         uint32_t immediateData : 1;
         uint32_t reserved : 2;
         uint32_t blockEventInterrupt : 1;
         uint32_t type : 6;
         uint32_t reserved2 : 16;
      };
   };
}__attribute__((packed))TRB;

typedef struct{
   union{
      struct{
         uint32_t r0;
         uint32_t r1;
         uint32_t r2;
         uint32_t r3;
      };
      struct{
         uint32_t bmRequestType : 8;
         uint32_t bRequest : 8;
         uint32_t wValue : 16;
         uint32_t wIndex : 16;
         uint32_t wLength : 16;
         uint32_t transferLength : 17;
         uint32_t reserved : 5;
         uint32_t interrupterTarget : 10;
         uint32_t cycleBit : 1;
         uint32_t reserved2 : 4;
         uint32_t interruptOnCompletion : 1;
         uint32_t immediateData : 1;
         uint32_t reserved3 : 3;
         uint32_t type : 6;
         uint32_t transferType : 2;
         uint32_t reserved4 : 14;
      };
   };
}__attribute__((packed))SetupStageTRB;

typedef struct{
   uint8_t bmRequestType;
   uint8_t bRequest;
   uint16_t wValue;
   uint16_t wIndex;
   uint16_t wLength;
}SetupStageHeader;

typedef struct{
   union{
      struct{
         uint32_t r0;
         uint32_t r1;
         uint32_t r2;
         uint32_t r3;
      };
      struct{
         uint64_t dataBuffer;

         uint32_t transferLength : 17;
         uint32_t tdSize : 5;
         uint32_t interrupterTarget : 10;

         uint32_t cycleBit : 1;
         uint32_t evalNextTRB : 1;
         uint32_t interruptOnShortPacked : 1;
         uint32_t noSnoop : 1;
         uint32_t chainBit : 1;
         uint32_t interruptOnCompletion : 1;
         uint32_t immediateData : 1;
         uint32_t reserved2 : 3;
         uint32_t type : 6;
         uint32_t direction : 1;
         uint32_t reserved3 : 15;
      };
   };
}__attribute__((packed))DataStageTRB;

typedef struct{
   union{
      struct{
         uint32_t r0;
         uint32_t r1;
         uint32_t r2;
         uint32_t r3;
      };
      struct{
         uint64_t reserved;
         uint32_t reserved2 : 22;
         uint32_t interrupterTarget : 10;
         uint32_t cycleBit : 1;
         uint32_t evaluateNextTrb : 1;
         uint32_t reserved3 : 2;
         uint32_t chainBit : 1;
         uint32_t interruptOnCompletion : 1;
         uint32_t reserved4 : 4;
         uint32_t type : 6;
         uint32_t direction : 1;
         uint32_t reserved5 : 15;

      };
   };
}__attribute__((packed))StatusStageTRB;


typedef struct{
   TRB trbs[3];
   int trbCount;
}TD;

typedef struct{
   uintptr_t address;
   int trbCount;
}Segment;

typedef struct{
   int pcs;
   TRB *dequeue;
}XhcdRing;

XhcdRing *xhcdRing_new(int trbCount);
void xhcdRing_free(XhcdRing *ring);
int xhcd_attachCommandRing(XhcHardware xhcHardware, XhcdRing *ring);
void xhcdRing_putTD(XhcdRing *ring, TD td);
void xhcdRing_putTRB(XhcdRing *ring, TRB trb);


TRB TRB_NOOP();
TRB TRB_ENABLE_SLOT(int slotType);
TRB TRB_ADDRESS_DEVICE(uint64_t inputContextAddr, uint32_t slotId, uint32_t bsr);
TRB TRB_EVALUATE_CONTEXT(void* inputContext, uint32_t slotId);
TRB TRB_CONFIGURE_ENDPOINT(void *inputContext, uint32_t slotId);
TRB TRB_NORMAL(void *dataBuffer, uint16_t bufferSize);

TRB TRB_SETUP_STAGE(SetupStageHeader header);
TRB TRB_DATA_STAGE(uint64_t dataBufferPointer, int bufferSize, uint8_t direction);
TRB TRB_STATUS_STAGE(uint8_t direction);

TD TD_GET_DESCRIPTOR(void *dataBufferPointer, int descriptorLengt);

#endif
