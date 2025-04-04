#ifndef XHCD_H_INCLUDED
#define XHCD_H_INCLUDED

#include "xhci.h"
#include "xhcd-registers.h"
#include "xhcd-ring.h"
#include "xhcd-event-ring.h"
#include "xhcd-hardware.h"
#include "threads.h"


typedef struct{
   XhcHardware hardware;

//    XhciCapabilities *capabilities;
//    XhciOperation *operation;
//    XhciDoorbell *doorbells;
//    InterrupterRegisters* interrupterRegisters;

   UsbPortInfo *portInfo;
   uint8_t enabledPorts;

   uint64_t *dcBaseAddressArray;
   uint32_t dcBaseAddressArraySize;

   XhcdRing *transferRing[16 + 1][31]; //indexed from 1 //FIXME
   XhcEventRing *eventRing;
   XhcdRing *commandRing;

   XhcInterruptHandler *handlers;

   volatile XhcEventTRB *eventBuffer;
   volatile uint32_t eventBufferSize;
   volatile uint32_t eventBufferDequeueIndex;
   volatile uint32_t eventBufferEnqueueIndex;

   Semaphore *eventSemaphore;
}Xhcd;


#endif
