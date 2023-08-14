#ifndef XHCD_EVENT_RING_H_INCLUDED
#define XHCD_EVENT_RING_H_INCLUDED

#include "stdint.h"
#include "xhcd-registers.h"

enum EventType{
   TransferEvent = 32,
   CommandCompletionEvent = 33,
   PortStatusChangeEvent = 34,
   BandwidthRequestEvent = 35,
   DoorbellEvent = 36,
   HostControllerEvent = 37,
   DeviceNotificationEvent = 38,
   MFINDEXWrapEvent = 39
};
enum CompletionCode{
   Invalid = 0,
   Success,
   DataBufferError,
   BabbleDetectedError,
   USBTransactionError,
   TrbError,
   StallError,
   ResourceError,
   BandwidthError,
   NoSlotsAvailiableError,
   InvalidStreamTypeError,
   SlotNotEnabledError,
   EndpointNotEnabledError,
   ShortPacket,
   RingUnderrun,
   RingOverrun,
   VFEventRingFullError,
   ParameterError,
   BandwithOverrunError,
   ContextStateError,
   NoPingResponseError,
   EventRingFullError,
   IncompatibleDeviceError,
   MissedServiceError,
   CommandRingStopped,
   CommandAborted,
   Stopped,
   StoppedLenghtInvalid,
   StoppedShortPacket,
   MaxExitLatencyTooLargeError,
   IsochBufferOverrun = 31,
   EventLostError,
   UndefinedError,
   InvalidStreamIdError,
   SecondaryBandwidthError,
   SplitTransactionError
};

typedef volatile struct{
   uint64_t baseAddress;
   uint32_t ringSegmentSize : 16; //p.515. n * 64B
   uint32_t reserved : 16;
   uint32_t reserved2;
}__attribute__((packed))EventRingSegmentTableEntry;

typedef volatile struct{
   uint32_t trbPointerLow;
   uint32_t trbPointerHigh;
   uint32_t trbTransferLength : 24;
   enum CompletionCode completionCode : 8;
   uint32_t cycleBit : 1;
   uint32_t reserved : 1;
   uint32_t evendData : 1;
   uint32_t reserved2 : 7;
   enum EventType trbType : 6;
   uint32_t endpointId : 5;
   uint32_t reserved3 : 3;
   uint32_t slotId : 8;
}__attribute__((packed))XhcEventTRB;

typedef struct{
   XhcEventTRB *dequeue;
   XhcEventTRB *segmentEnd;
   int ccs;

   EventRingSegmentTableEntry *currSegment;
   EventRingSegmentTableEntry *segmentTableEnd;
   InterrupterRegisters *interruptor;

   int segmentCount;
}XhcEventRing;


XhcEventRing xhcd_newEventRing(int trbCount);

int xhcd_attachEventRing(XhcEventRing *ring, InterrupterRegisters *interruptor);
int xhcd_readEvent(XhcEventRing *ring, XhcEventTRB* result, int maxOutput);

int hasPendingEvent(XhcEventRing *eventRing);
#endif
