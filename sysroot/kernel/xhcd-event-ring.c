#include "kernel/xhcd-event-ring.h"
#include "stdlib.h"
#include "stdio.h"

//FIXME: not all TRB slots are used in a segment

#define INTERRUPTOR_SEGMENT_TABLE_SIZE_OFFSET 0x08
#define INTERRUPTOR_SEGMENT_TABLE_OFFSET 0x10
#define INTERRUPTOR_DEQUEUE_OFFSET 0x18

#define EVENT_HANDLER_BUSY_BIT (1 << 3)

static int initSegment(XhcEventRingSegment segment);

static int incrementDequeue(XhcEventRing *eventRing);
static int advanceERDP(XhcEventRing *eventRing);
static int getERSTIndex(XhcEventRing *eventRing);
static int incrementSegment(XhcEventRing *eventRing);

XhcEventRing xhcd_newEventRing(XhcEventRingSegment *segments,
                  int segmentCount,
                  EventRingSegmentTableEntry2 *segmentTable){
   for(int i = 0; i < segmentCount; i++){
      initSegment(segments[i]);
      uint64_t address = (uint64_t)segments[i].basePointer;
      segmentTable[i].baseAddressLow = (uint32_t) address;
      segmentTable[i].baseAddressHigh = (uint32_t)(address >> 32);
      segmentTable[i].ringSegmentSize = segments[i].trbCount;
      printf("address %X %X\n", address);
   }
   XhcEventRing ring;
   ring.currSegment = segmentTable;
   ring.segmentTableEnd = segmentTable + segmentCount;
   ring.segmentCount = segmentCount;
   ring.dequeue = (XhcEventTRB*)segments[0].basePointer;
   ring.segmentEnd = ring.dequeue + segments[0].trbCount;
   ring.ccs = 1;
   return ring;
}

int xhcd_attachEventRing(XhcEventRing *ring, InterrupterRegisters *interruptor){
   ring->interruptor = interruptor;
   interruptor->eventRingSegmentTableSize = ring->segmentCount;
   uint64_t dequeAddr = (uint64_t)ring->dequeue;
   interruptor->eventRingDequePointerLow = (uint32_t)dequeAddr;
   interruptor->eventRingDequePointerHigh = (uint32_t)(dequeAddr >> 32);
   interruptor->eventRingSegmentTableAddress = (uint64_t)ring->currSegment;
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
      uint64_t low = eventRing->currSegment->baseAddressLow;
      uint64_t high = eventRing->currSegment->baseAddressHigh;
      eventRing->dequeue = (XhcEventTRB*)((high << 32) | low);
      eventRing->segmentEnd = eventRing->dequeue + eventRing->currSegment->ringSegmentSize;
   }
   return 1;
}
static int advanceERDP(XhcEventRing *eventRing){
   uint32_t dequeERSTSegmentIndex = getERSTIndex(eventRing) & 0b111;
   uint64_t dequeAddr = (uint64_t)eventRing->dequeue;
   uint32_t low = (uint32_t)dequeAddr | dequeERSTSegmentIndex | EVENT_HANDLER_BUSY_BIT;
   uint32_t high = (uint32_t)(dequeAddr >> 32);
   eventRing->interruptor->eventRingDequePointerLow = low;
   eventRing->interruptor->eventRingDequePointerHigh = high;
   return 1;
}
static int getERSTIndex(XhcEventRing *eventRing){
   EventRingSegmentTableEntry2 *base =
         (EventRingSegmentTableEntry2 *)eventRing->interruptor->eventRingSegmentTableAddress;
   return eventRing->currSegment - base;
}
static int incrementSegment(XhcEventRing *eventRing){
   eventRing->currSegment++;
   if(eventRing->currSegment == eventRing->segmentTableEnd){
      uint64_t addr = eventRing->interruptor->eventRingSegmentTableAddress;
      eventRing->currSegment = (EventRingSegmentTableEntry2*)addr;
      eventRing->ccs = !eventRing->ccs;
      //printf("wrap: %X\n", eventRing->ccs);
   }

   return 1;
}

static int initSegment(XhcEventRingSegment segment){
   memset((void*)segment.basePointer, 0, segment.trbCount * sizeof(XhcEventTRB));
   return 1;
}
