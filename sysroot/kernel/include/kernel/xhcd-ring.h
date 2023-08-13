#ifndef XHCD_RING_H_INCLUDED
#define XHCD_RING_H_INCLUDED

#include "stdint.h"
typedef volatile struct{
   uint32_t ringSegmentLow;
   uint32_t ringSegmentHigh;
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
}__attribute__((packed))LinkTRB2;

typedef volatile struct{
   union{
      struct{
         uint32_t r0;
         uint32_t r1;
         uint32_t r2;
         uint32_t r3; };
      struct{
         uint32_t dataBufferPointerLow;
         uint32_t dataBufferpointerHigh;
         uint32_t trbTransferLength : 17;
         uint32_t tdSize : 5;
         uint32_t interrupterTarget : 10;
         uint32_t cycleBit : 1;
         uint32_t other : 9;
         uint32_t trbType : 6;
         uint32_t reserved : 16;
      };
   };
}__attribute__((packed))TRB2;

typedef volatile struct{
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

typedef volatile struct{
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

typedef volatile struct{
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
   TRB2 trbs[3];
}TD;

typedef struct{
   uint64_t address;
   int trbCount;
}Segment;

typedef struct{
   int pcs;
   TRB2 *dequeue;
}XhcdRing;

XhcdRing xhcd_newRing(Segment* segments, int count);
int xhcd_attachCommandRing(uint32_t* operationalBase, XhcdRing *ring);
void xhcd_putTD(TD td, XhcdRing *ring);
void xhcd_putTRB(TRB2 trb, XhcdRing *ring);


TRB2 TRB_NOOP();
TRB2 TRB_ENABLE_SLOT(int slotType);
TRB2 TRB_ADDRESS_DEVICE(uint64_t inputContextAddr, uint32_t slotId, uint32_t bsr);
TRB2 TRB_EVALUATE_CONTEXT(void* inputContext, uint32_t slotId);
TD TD_GET_DESCRIPTOR(void *dataBufferPointer, int descriptorLengt);







#endif
