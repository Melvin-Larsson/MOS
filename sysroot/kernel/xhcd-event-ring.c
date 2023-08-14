#include "kernel/xhcd-event-ring.h"
#include "stdlib.h"
#include "stdio.h"

//FIXME: not all TRB slots are used in a segment

#define INTERRUPTOR_SEGMENT_TABLE_SIZE_OFFSET 0x08
#define INTERRUPTOR_SEGMENT_TABLE_OFFSET 0x10
#define INTERRUPTOR_DEQUEUE_OFFSET 0x18

#define EVENT_HANDLER_BUSY_BIT (1 << 3)

static int incrementDequeue(XhcEventRing *eventRing);
static int advanceERDP(XhcEventRing *eventRing);
static int getERSTIndex(XhcEventRing *eventRing);
static int incrementSegment(XhcEventRing *eventRing);

XhcEventRing xhcd_newEventRing(int trbCount){
   EventRingSegmentTableEntry *segmentTable = mallocco(sizeof(EventRingSegmentTableEntry), 64, 0);
   void* segmentPtr = callocco(sizeof(XhcEventTRB) * trbCount, 64, 64000);

   segmentTable->baseAddress = (uintptr_t)segmentPtr;
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

int xhcd_attachEventRing(XhcEventRing *ring, InterrupterRegisters *interruptor){
   ring->interruptor = interruptor;
   interruptor->eventRingSegmentTableSize = ring->segmentCount;
   uintptr_t dequeAddr = (uintptr_t)ring->dequeue;
   interruptor->eventRingDequePointer = dequeAddr;
   interruptor->eventRingSegmentTableAddress = (uintptr_t)ring->currSegment;
   return 1;
}
int xhcd_readEvent(XhcEventRing *ring, XhcEventTRB* result, int maxOutput){
   int i = 0;
   for(; i < maxOutput && hasPendingEvent(ring); i++){
      result[i] = *ring->dequeue;
      incrementDequeue(ring);
   }
   advanceERDP(ring);
   return i;

}
int hasPendingEvent(XhcEventRing *eventRing){
   return eventRing->dequeue->cycleBit == eventRing->ccs;
}
static int incrementDequeue(XhcEventRing *eventRing){
//   printf("deq %X -> %X ", eventRing->dequeue, eventRing->segmentEnd);
   eventRing->dequeue++;
   if(eventRing->dequeue == eventRing->segmentEnd){
//      printf("wrap %X\n", eventRing->dequeue);
      incrementSegment(eventRing);
      uintptr_t ptr = eventRing->currSegment->baseAddress;
      eventRing->dequeue = (XhcEventTRB*)ptr;
      eventRing->segmentEnd = eventRing->dequeue + eventRing->currSegment->ringSegmentSize;
   }
   return 1;
}
static int advanceERDP(XhcEventRing *eventRing){
   uint32_t dequeERSTSegmentIndex = getERSTIndex(eventRing) & 0b111;
   uintptr_t dequeAddr = (uintptr_t)eventRing->dequeue;
   eventRing->interruptor->eventRingDequePointer =
      dequeAddr |
      dequeERSTSegmentIndex |
      EVENT_HANDLER_BUSY_BIT;

   
   return 1;
}
static int getERSTIndex(XhcEventRing *eventRing){
   uintptr_t ptr = (uintptr_t)eventRing->interruptor->eventRingSegmentTableAddress;
   EventRingSegmentTableEntry *base =
         (EventRingSegmentTableEntry *)ptr;
   return eventRing->currSegment - base;
}
static int incrementSegment(XhcEventRing *eventRing){
   eventRing->currSegment++;
   if(eventRing->currSegment == eventRing->segmentTableEnd){
      uintptr_t addr = eventRing->interruptor->eventRingSegmentTableAddress;
      eventRing->currSegment = (EventRingSegmentTableEntry*)addr;
      eventRing->ccs = !eventRing->ccs;
      //printf("wrap: %X\n", eventRing->ccs);
   }

   return 1;
}
