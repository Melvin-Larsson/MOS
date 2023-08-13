#include "kernel/xhcd-ring.h"
#include "stdio.h"
#include "stdlib.h"

#define DEFAULT_PCS 1

#define CRCR_OFFSET 0x18

#define TRB_TYPE_LINK 6
#define TRB_TYPE_NOOP 23
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_EVALUATE_CONTEXT 13
#define TRB_TYPE_SETUP 2
#define TRB_TYPE_DATA 3
#define TRB_TYPE_STATUS 4


#define TRB_SLOT_TYPE_POS 16
#define TRB_SLOT_ID_POS 24
#define TRB_TYPE_POS 10

#define DIRECTION_IN 1
#define DIRECTION_OUT 0

#define ADDRESS_TRB_BSR_POS 9

#define TRANSFER_TYPE_IN 3

#define REQUEST_GET_DESCRIPTOR 6

static void initSegment(Segment segment, Segment nextSegment, int isLast);

XhcdRing xhcd_newRing(Segment* segments, int count){
   XhcdRing ring;
   ring.pcs = DEFAULT_PCS;
   uint64_t startAddress = segments[0].address;
   ring.dequeue = (TRB2 *)startAddress;
   for(int i = 0; i < count - 1; i++){
      initSegment(segments[i], segments[i+1], 0);
   }
   initSegment(segments[count - 1], segments[0], 1);
   return ring;
}
int xhcd_attachCommandRing(uint32_t* operationalBase, XhcdRing *ring){
   uint32_t *commandRingControll = operationalBase + CRCR_OFFSET / 4;
   uint64_t address = (uint64_t)ring->dequeue;
   *commandRingControll  = ((uint32_t)address | ring->pcs);
   *(commandRingControll + 1) = (uint32_t)(address >> 32);
   return 1;
}
void xhcd_putTD(TD td, XhcdRing *ring){
   for(int i = 0; i < 3; i++){
      xhcd_putTRB(td.trbs[i], ring);
   }
}
void xhcd_putTRB(TRB2 trb, XhcdRing *ring){
   trb.cycleBit = ring->pcs;
   *ring->dequeue = trb; 
   ring->dequeue++;
   if(ring->dequeue->trbType == TRB_TYPE_LINK){
      LinkTRB2 *link = (LinkTRB2*)ring->dequeue;
      link->cycleBit = ring->pcs;
      uint64_t low = (uint64_t)link->ringSegmentLow;
      uint64_t high = (uint64_t)link->ringSegmentHigh;
      ring->dequeue = (TRB2*)(low | (high << 32));
      ring->pcs ^= link->toggleCycle;
   }
}
TRB2 TRB_NOOP(){
   TRB2 trb = {{{0,0,0,0}}};
   trb.r3 = TRB_TYPE_NOOP << TRB_TYPE_POS;
   return trb;
}
TRB2 TRB_ENABLE_SLOT(int slotType){
   TRB2 trb = {{{0,0,0,0}}};
   trb.r3 = TRB_TYPE_ENABLE_SLOT << TRB_TYPE_POS |
            slotType << TRB_SLOT_TYPE_POS;
   return trb;
}
TRB2 TRB_ADDRESS_DEVICE(uint64_t inputContextAddr, uint32_t slotId, uint32_t bsr){
   TRB2 trb = {{{0,0,0,0}}};
   trb.r0 = (uint32_t)inputContextAddr;
   trb.r1 = (uint32_t)(inputContextAddr >> 32);
   trb.r3 = bsr << ADDRESS_TRB_BSR_POS |
            TRB_TYPE_ADDRESS_DEVICE << TRB_TYPE_POS |
            slotId << TRB_SLOT_ID_POS;
   return trb;
}
TRB2 TRB_EVALUATE_CONTEXT(void* inputContext, uint32_t slotId){
   TRB2 trb = {{{0,0,0,0}}};
   uint64_t addr = (uint64_t)inputContext;
   trb.r0 = (uint32_t)addr;
   trb.r1 = (uint32_t)(addr >> 32);
   trb.r3 = TRB_TYPE_EVALUATE_CONTEXT << TRB_TYPE_POS |
            slotId << TRB_SLOT_ID_POS;
   return trb;

}
TD TD_GET_DESCRIPTOR(void *dataBufferPointer, int descriptorLength){
   SetupStageTRB setupTrb = {{{0,0,0,0}}};
   setupTrb.type = TRB_TYPE_SETUP;
   setupTrb.transferType = TRANSFER_TYPE_IN;
   setupTrb.transferLength = 8;
   setupTrb.interruptOnCompletion = 0;
   setupTrb.immediateData = 1;
   setupTrb.bmRequestType = 0x80;
   setupTrb.bRequest = REQUEST_GET_DESCRIPTOR;
   setupTrb.wValue = 0x0100;
   setupTrb.wIndex = 0;
   setupTrb.wLength = descriptorLength;
   
   DataStageTRB dataTrb = {{{0,0,0,0}}};
   dataTrb.type = TRB_TYPE_DATA;
   dataTrb.direction = DIRECTION_IN;
   dataTrb.transferLength = descriptorLength;
   dataTrb.chainBit = 0;
   dataTrb.interruptOnCompletion = 0;
   dataTrb.immediateData = 0;
   dataTrb.dataBuffer = (uint64_t)dataBufferPointer;

   StatusStageTRB statusTrb = {{{0,0,0,0}}};
   statusTrb.type = TRB_TYPE_STATUS;
   statusTrb.direction = DIRECTION_OUT;
   statusTrb.chainBit = 0;
   statusTrb.interruptOnCompletion = 1;

   TRB2 *t1 = (TRB2*)&setupTrb;
   TRB2 *t2 = (TRB2*)&dataTrb;
   TRB2 *t3 = (TRB2*)&statusTrb;
   
   TD td = {{*t1, *t2, *t3}};
   return td;
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
