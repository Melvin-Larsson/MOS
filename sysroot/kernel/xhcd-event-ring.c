#include "kernel/paging.h"
#include "kernel/xhcd-event-ring.h"
#include "stdlib.h"
#include "stdio.h"

//FIXME: not all TRB slots are used in a segment

#define INTERRUPTOR_SEGMENT_TABLE_SIZE_OFFSET 0x08
#define INTERRUPTOR_SEGMENT_TABLE_OFFSET 0x10
#define INTERRUPTOR_DEQUEUE_OFFSET 0x18

#define EVENT_HANDLER_BUSY_BIT (1 << 3)

static int incrementDequeue(XhcHardware xhc, XhcEventRing *eventRing);
static int advanceERDP(XhcHardware xhc, XhcEventRing *eventRing);
static int getERSTIndex(XhcHardware xhc, XhcEventRing *eventRing);
static int incrementSegment(XhcHardware xhc, XhcEventRing *eventRing);

XhcEventRing xhcd_newEventRing(int trbCount){
//    EventRingSegmentTableEntry *segmentTable = mallocco(sizeof(EventRingSegmentTableEntry), 64, 0);
   unsigned int size = sizeof(XhcEventTRB) * trbCount + sizeof(XhcEventTRB) * trbCount % 64;
   printf("Size: %X\n", size);
   void* segmentPtr = callocco(size, 64, 64000);

   EventRingSegmentTableEntry *segmentTable = callocco(64, 64, 0);

   segmentTable->baseAddress = paging_getPhysicalAddress((uintptr_t)segmentPtr);
   segmentTable->ringSegmentSize = trbCount;

   XhcEventRing ring;
   ring.currSegment = segmentTable;
   ring.segmentTableEnd = segmentTable + 1;
   ring.segmentCount = 1;
   ring.dequeue = (XhcEventTRB*)segmentPtr;
   ring.segmentEnd = ring.dequeue + trbCount;
   ring.ccs = 1;
   return ring;
}

int xhcd_attachEventRing(XhcHardware xhc, XhcEventRing *ring, int interruptorIndex){
   uintptr_t dequeAddr = paging_getPhysicalAddress((uintptr_t)ring->dequeue);
   uintptr_t tableAddr = paging_getPhysicalAddress((uintptr_t)ring->currSegment);

   xhcd_writeInterrupter(xhc, interruptorIndex, ERSTSZ, ring->segmentCount);
   xhcd_writeInterrupter(xhc, interruptorIndex, ERDP, dequeAddr);
   xhcd_writeInterrupter(xhc, interruptorIndex, ERSTBA, tableAddr);

   ring->interrupterIndex = interruptorIndex;
   ring->xhc = xhc;

   return 1;
}

int xhcd_readEvent(XhcEventRing *ring, XhcEventTRB* result, int maxOutput){
   int i = 0;
   for(; i < maxOutput && hasPendingEvent(ring); i++){
      result[i] = *ring->dequeue;
      incrementDequeue(ring->xhc, ring);
   }
   if(i){
      advanceERDP(ring->xhc, ring);
   }
   return i;
}
int hasPendingEvent(XhcEventRing *eventRing){
   return eventRing->dequeue->cycleBit == eventRing->ccs;
}
static int incrementDequeue(XhcHardware xhc, XhcEventRing *eventRing){
//   printf("deq %X -> %X ", eventRing->dequeue, eventRing->segmentEnd);
   eventRing->dequeue++;
   if(eventRing->dequeue == eventRing->segmentEnd){
//      printf("wrap %X\n", eventRing->dequeue);
      incrementSegment(xhc, eventRing);
      uintptr_t ptr = eventRing->currSegment->baseAddress;
      eventRing->dequeue = (XhcEventTRB*)ptr;
      eventRing->segmentEnd = eventRing->dequeue + eventRing->currSegment->ringSegmentSize;
   }
   return 1;
}
static int advanceERDP(XhcHardware xhc, XhcEventRing *eventRing){
   uint32_t dequeERSTSegmentIndex = getERSTIndex(xhc, eventRing) & 0b111;
   uintptr_t dequeAddr = paging_getPhysicalAddress((uintptr_t)eventRing->dequeue);
   xhcd_writeInterrupter(xhc, eventRing->interrupterIndex, ERDP,
      dequeAddr |
      dequeERSTSegmentIndex |
      EVENT_HANDLER_BUSY_BIT);

   return 1;
}
static int getERSTIndex(XhcHardware xhc, XhcEventRing *eventRing){
   uintptr_t ptr = (uintptr_t)xhcd_readInterrupter(xhc, eventRing->interrupterIndex, ERSTBA);
   EventRingSegmentTableEntry *base =
         (EventRingSegmentTableEntry *)ptr;
   return eventRing->currSegment - base;
}
static int incrementSegment(XhcHardware xhc, XhcEventRing *eventRing){
   eventRing->currSegment++;
   if(eventRing->currSegment == eventRing->segmentTableEnd){
      uintptr_t addr = xhcd_readInterrupter(xhc, eventRing->interrupterIndex, ERSTBA);
      eventRing->currSegment = (EventRingSegmentTableEntry*)addr;
      eventRing->ccs = !eventRing->ccs;
      //printf("wrap: %X\n", eventRing->ccs);
   }

   return 1;
}
