#include "kernel/paging.h"
#include "stdint.h"
#include "kernel/xhcd-event-ring.h"
#include "kernel/logging.h"
#include "kernel/memory.h"
#include "stdlib.h"
#include "utils/assert.h"

//FIXME: not all TRB slots are used in a segment

#define INTERRUPTOR_SEGMENT_TABLE_SIZE_OFFSET 0x08
#define INTERRUPTOR_SEGMENT_TABLE_OFFSET 0x10
#define INTERRUPTOR_DEQUEUE_OFFSET 0x18

#define EVENT_HANDLER_BUSY_BIT (1 << 3)

#define ROUND_UP_TO(x, y) ((((x) + (y) - 1) / (y)) * (y))
#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define MAX_EVENT_RING_SEGMENT_TRB_COUNT 4096

static int incrementDequeue(XhcHardware xhc, XhcEventRing *eventRing);
static int advanceERDP(XhcHardware xhc, XhcEventRing *eventRing);
static int getERSTIndex(XhcHardware xhc, XhcEventRing *eventRing);
static int incrementSegment(XhcHardware xhc, XhcEventRing *eventRing);

XhcEventRing* xhcdEventRing_new(size_t trbCount){
   assert(trbCount >= 16);

   uint32_t segmentCount = DIV_ROUND_UP(trbCount, MAX_EVENT_RING_SEGMENT_TRB_COUNT);
   size_t segmentTableSize = ROUND_UP_TO(segmentCount * sizeof(EventRingSegmentTableEntry), 64);
   EventRingSegmentTableEntry *segmentTable = kcallocco(segmentTableSize, 64, 0);
   if(!segmentTable){
      loggWarning("Unable to allocate segment table");
      return 0;
   }

   loggDebug("Init event ring with %d trbs, using %d segments. Table at %X", trbCount, segmentCount, segmentTable);

   uint32_t segmentTrbCount = DIV_ROUND_UP(trbCount, segmentCount);

   for(size_t i = 0; i < segmentCount; i++){
      uint32_t currSegmentTrbCount = MIN(segmentTrbCount, trbCount);
      size_t size = currSegmentTrbCount * sizeof(XhcEventTRB);
      void *segmentAddress = kmallocco(size, 64, 0);
      loggDebug("Creating event segment of size %d at %X", currSegmentTrbCount, segmentAddress);
      if(segmentAddress == 0){
         loggWarning("Unable to free allocated segments");
         kfree((void*)segmentTable);
         return 0;
      }
      memset(segmentAddress, 0, size);
      segmentTable[i].baseAddress = paging_getPhysicalAddress((uintptr_t)segmentAddress);
      segmentTable[i].ringSegmentSize = currSegmentTrbCount;

      assert(segmentTable[i].baseAddress == (uintptr_t)segmentAddress); //FIXME: Other code relies on this, but it is wrong

      trbCount -= segmentTrbCount;
   }


   XhcEventRing *ring = kmalloc(sizeof(XhcEventRing));
   if(!ring){
      loggWarning("Unable to allocate event ring");
      loggWarning("Unable to free allocated segments");
      kfree((void*)segmentTable);
      return 0;
   }

   loggDebug("Creating event ring at %X", ring);
   ring->currSegment = &segmentTable[0];
   ring->segmentTableEnd = &segmentTable[segmentCount]; //Points to the element after the last entry
   ring->segmentCount = segmentCount;
   ring->dequeue = (XhcEventTRB *)(uintptr_t)segmentTable[0].baseAddress; //FIXME: This should be the logical address and not physical. This only works if the mapping is direct
   ring->segmentEnd = ring->dequeue + segmentTable[0].ringSegmentSize;
   ring->ccs = 1;

   return ring;
}

static size_t getMaxSegmentCount(XhcHardware xhc){
   StructParams2 params = {.bits = xhcd_readCapability(xhc, HCSPARAMS2)};
   return (1 << params.erstMax);
}

size_t xhcdEventRing_getMaxTrbCount(XhcHardware xhc){
   return getMaxSegmentCount(xhc) * MAX_EVENT_RING_SEGMENT_TRB_COUNT;
}

void xhcdEventRing_free(XhcEventRing *eventRing){
   loggWarning("Unable to free event ring");
   kfree(eventRing);
}

void xhcdEventRing_attach(XhcHardware xhc, XhcEventRing *ring, int interruptorIndex){
   assert(ring->segmentCount <= getMaxSegmentCount(xhc));
   assert(interruptorIndex >= 0 && interruptorIndex <= 1023);
   assert(interruptorIndex != 0 || ring->segmentCount != 0);
   assert(interruptorIndex != 0 || (xhcd_readRegister(xhc, USBStatus) & USBSTS_HCH_BIT) == USBSTS_HCH_BIT);

   loggDebug("Attach event ring");

   uintptr_t dequeAddr = paging_getPhysicalAddress((uintptr_t)ring->dequeue);
   uintptr_t tableAddr = paging_getPhysicalAddress((uintptr_t)ring->currSegment);

   loggDebug("Dequeue:  %X, Table: %X", dequeAddr, tableAddr);

   xhcd_writeInterrupter(xhc, interruptorIndex, ERSTSZ, ring->segmentCount);
   xhcd_writeInterrupter(xhc, interruptorIndex, ERDP, dequeAddr);
   xhcd_writeInterrupter(xhc, interruptorIndex, ERSTBA, tableAddr);

   loggDebug("Event ring attached");

   ring->interrupterIndex = interruptorIndex;
   ring->xhc = xhc;
}

int xhcdEventRing_read(XhcEventRing *ring, XhcEventTRB* result, int maxOutput){
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
