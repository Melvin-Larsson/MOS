#include "kernel/xhcd-ring.h"
#include "stdio.h"
#include "stdlib.h"

#define DEFAULT_PCS 1

#define CRCR_OFFSET 0x18

#define TRB_TYPE_LINK 6
#define TRB_TYPE_NOOP 23

static void initSegment(Segment segment, Segment nextSegment, int isLast);

XhcdRing xhcd_newRing(Segment* segments, int count){
   XhcdRing ring;
   ring.pcs = DEFAULT_PCS;
   ring.dequeue = (TRB2*)segments[0].address;
   for(int i = 0; i < count - 1; i++){
      initSegment(segments[i], segments[i+1], 0);
   }
   initSegment(segments[count - 1], segments[0], 1);
   return ring;
}
int xhcd_attachCommandRing(uint32_t* operationalBase, XhcdRing *ring){
   uint32_t *commandRingControll = operationalBase + CRCR_OFFSET;
   *commandRingControll  = ((uint32_t)ring->dequeue | ring->pcs);
   *(commandRingControll + 1) = (uint32_t)((uint64_t)ring->dequeue >> 32);
   return 1;
}
void xhcd_putTRB(TRB2* trb, XhcdRing *ring){
   trb->cycleBit = ring->pcs;
   ring->dequeue = trb; 
   ring->dequeue++;
   if(ring->dequeue->trbType == TRB_TYPE_LINK){
      LinkTRB2 *link = (LinkTRB2*)ring->dequeue;
      uint64_t low = (uint64_t)link->ringSegmentLow;
      uint64_t high = (uint64_t)link->ringSegmentHigh;
      ring->dequeue = (TRB2*)(low | (high << 32));
      ring->pcs ^= link->toggleCycle;
   }
}
TRB2 TRB_NOOP(){
   TRB2 trb = {{{0,0,0,0}}};
   trb.trbType = TRB_TYPE_NOOP;
   return trb;

}
static void initSegment(Segment segment, Segment nextSegment, int isLast){
   memset((void*)segment.address, 0, segment.trbCount * sizeof(TRB2));
   TRB2 *trbs = (TRB2*)segment.address;
   if(!DEFAULT_PCS){
      for(int i = 0; i < segment.trbCount; i ++){
         trbs[i].r3 |= DEFAULT_PCS; 
      }
   }
   LinkTRB2 *link = (LinkTRB2*)&trbs[segment.trbCount - 1];
   link->ringSegmentLow = (uint32_t)nextSegment.address;
   link->ringSegmentHigh = (uint32_t)(nextSegment.address >> 32);
   link->cycleBit = DEFAULT_PCS;
   link->toggleCycle = isLast;
   link->trbType = TRB_TYPE_LINK;
}
