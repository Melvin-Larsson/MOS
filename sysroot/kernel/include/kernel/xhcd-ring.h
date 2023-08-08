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
         uint32_t r3;
      };
      struct{
         uint32_t dataBufferPointerLow;
         uint32_t dataBufferpointerHigh;
         uint32_t trbTransferLength : 16;
         uint32_t tdSize : 4;
         uint32_t interrupterTarget : 10;
         uint32_t cycleBit : 1;
         uint32_t other : 9;
         uint32_t trbType : 6;
         uint32_t reserved : 16;
      };
   };
}__attribute__((packed))TRB2;

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
void xhcd_putTRB(TRB2* trb, XhcdRing *ring);


TRB2 TRB_NOOP();









#endif
