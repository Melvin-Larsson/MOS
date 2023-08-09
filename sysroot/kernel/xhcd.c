#include "kernel/xhcd.h"
#include "kernel/xhcd-ring.h"
#include "kernel/xhcd-event-ring.h"
#include "stdio.h"
#include "stdlib.h"

//DC = Device context
//p.168. TRB rings shall not cross 64KB boundary
//p82 UBS LS FS HS requres port process to advance to enabled state

#define CNR_FLAG (1<<11)

#define MAX_DEVICE_SLOTS_ENABLED 16
#define DEFAULT_COMMAND_RING_SIZE 32
#define DEFAULT_EVENT_SEGEMNT_TRB_COUNT 16
#define DEFAULT_TRANSFER_RING_TRB_COUNT 16

#define USBCMD_RUN_STOP_BIT 1

#define LINK_STATE_RX_DETECT 5
#define LINK_STATE_POLLING 7

#define EXTENDED_CAPABILITY_SUPPORTED_PROTOCOL 2

#define DEFAULT_PCS 1

#define ENDPOINT_TYPE_CONTROL 4


#define INPUT_CONTEXT_A0A1_MASK 0b11

static int initBasePointers(PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
static void waitForControllerReady(Xhci *xhci);
static void setMaxEnabledDeviceSlots(Xhci *xhci);
static void initDCAddressArray(Xhci *xhci);
static void turnOnController(Xhci *xhci);
static void initScratchPad(Xhci *xhci);

static int getSlotType(Xhci *xhci);
static void ringCommandDoorbell(Xhci *xhci);

static void test(Xhci *xhci);

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portNumber);
//static int shouldEnablePort(Xhci *xhci, int portNumber);

__attribute__((aligned(64)))
static volatile uint64_t dcBaseAddressArray[MAX_DEVICE_SLOTS_ENABLED + 1];

__attribute__((aligned(64)))
static XhcInputContext inputContext[MAX_DEVICE_SLOTS_ENABLED];

__attribute__((aligned(64)))
static XhcOutputContext outputContext[MAX_DEVICE_SLOTS_ENABLED];

__attribute((aligned(64)))
static volatile TRB commandRing[DEFAULT_COMMAND_RING_SIZE];

__attribute__((aligned(64)))
static volatile EventRingSegmentTableEntry eventRingSegmentTable[1];

__attribute__((aligned(64)))
static volatile CommandCompletionEventTRB eventRing[DEFAULT_EVENT_SEGEMNT_TRB_COUNT];

static volatile uint64_t scratchpadPointers[255];

__attribute__((aligned(4096)))
static volatile uint8_t scratchpadStart[139264];

__attribute__((aligned(4096)))
static TRB transferRing[DEFAULT_TRANSFER_RING_TRB_COUNT];

XhcdRing commandRingDescriptor;
XhcdRing transferRingDescriptor;
XhcEventRing eventRingDescriptor;

int xhcd_init(PciGeneralDeviceHeader *pciHeader, Xhci *xhci){
   while(1);
   int errorCode = 0;
   if((errorCode = initBasePointers(pciHeader, xhci)) != 0){
      return errorCode;
   }
   waitForControllerReady(xhci);
   setMaxEnabledDeviceSlots(xhci);
   initDCAddressArray(xhci);
   
   Segment segments[] = {{(uint64_t)commandRing, DEFAULT_COMMAND_RING_SIZE}};
   commandRingDescriptor = xhcd_newRing(segments, 1);
   xhcd_attachCommandRing((uint32_t *)xhci->operation, &commandRingDescriptor);

   XhcEventRingSegment eventSegments[] = {{(uint32_t*)eventRing, DEFAULT_EVENT_SEGEMNT_TRB_COUNT}};
   eventRingDescriptor = xhcd_newEventRing(eventSegments, 1, (EventRingSegmentTableEntry2*)eventRingSegmentTable);
   xhcd_attachEventRing(&eventRingDescriptor, &xhci->interrupterRegisters[0]);

   initScratchPad(xhci);
   turnOnController(xhci);

   printf("xhc running %b\n", xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT);
   printf("xhc halted %b\n", xhci->operation->USBStatus & 1);
   printf("Error? %d\n", xhci->operation->USBStatus & (1 << 12));

   return 0;
}
int xhcd_checkForDeviceAttach(Xhci *xhci){  
   StructParams1 structParams1 = xhci->capabilities->structParams1;
   int count = structParams1.maxPorts;
   XhciOperation *operation = xhci->operation;
   for(int i = 0; i < count; i++){
      XhciPortRegisters *port = &operation->ports[i];
      PortStatusAndControll status = port->statusAndControll;
      if(/*status.connectStatusChange &&*/ status.currentConnectStatus){
         status.connectStatusChange = 1;
            *(&port->statusAndControll) = status;
         return i;
      }
   }
   return -1;
}
int xhcd_enable(Xhci *xhci, int portIndex){
   if(xhcd_isPortEnabled(xhci, portIndex)){
      printf("Port already enabled (USB3)\n");
      return 1;
   }
/*   if(!shouldEnablePort(xhci, portNumber)){ //FIXME: This should worrk
      printf("Failed to enable port (USB3)\n");
      return 0;
   }*/
   printf("Enabling port (USB2)\n");
   PortStatusAndControll *statusAndControll = getPortStatus(xhci, portIndex);
   PortStatusAndControll temp = *statusAndControll;
   temp.portReset = 1;
   *statusAndControll = temp;
   while(!temp.portResetChange){
      temp = *statusAndControll;
   }
   temp.portResetChange = 1; //Clear
   *statusAndControll = temp; 
   int isEnabled = xhcd_isPortEnabled(xhci, portIndex);
   if(!isEnabled){
      return 0;
   }

   return 1;
}
int xhcd_initPort(Xhci *xhci, int portIndex){
   xhcd_putTRB(TRB_ENABLE_SLOT(getSlotType(xhci)), &commandRingDescriptor);
   ringCommandDoorbell(xhci);
   XhcEventTRB trb;
   while(!xhcd_readEvent(&eventRingDescriptor, &trb, 1));
   if(trb.trbType != CommandCompletionEvent){
      printf("[xhc] unknown event\n");
      return -1;
   }
   if(trb.completionCode == NoSlotsAvailiableError){
      printf("[xhc] no slots availiable\n");
      return -1;
   }
   if(trb.completionCode != Success){
      printf("[xhc] something went wrong (initPort)");
      return -1;
   }
   printf("event trb : %X %X %X %X\n", trb);
   printf("[xhc] TRB slot id: %X\n", trb.slotId);
   int slotId = trb.slotId - 1; //FIXME: why -1? seems like qemu enables devices itself (bios?)
   memset((void*)&inputContext[slotId], 0, sizeof(XhcInputContext));
   inputContext[slotId].inputControlContext.addContextFlags |= INPUT_CONTEXT_A0A1_MASK;
   
   XhcSlotContext *slotContext = &inputContext[slotId].slotContext;
   XhcSlotContext tempSlotContext = *slotContext;
   tempSlotContext.rootHubPortNumber = portIndex + 1; //port number is 1 indexed
   tempSlotContext.routeString = 0;
   tempSlotContext.contextEntries = 1;
   *slotContext = tempSlotContext;

   Segment transferRingSegments[] = {{(uint64_t)transferRing, DEFAULT_TRANSFER_RING_TRB_COUNT}};
   transferRingDescriptor = xhcd_newRing(transferRingSegments, 1);

   XhcEndpointContext *controlEndpoint = &inputContext[slotId].endpointContext[0];
   XhcEndpointContext tempControlEndpoint = *controlEndpoint;
   tempControlEndpoint.endpointType = ENDPOINT_TYPE_CONTROL;
   tempControlEndpoint.maxPackedSize = 8; //FIXME: what value?
   tempControlEndpoint.maxBurstSize = 0;
   tempControlEndpoint.dequeuePointer = (uint64_t)transferRingDescriptor.dequeue | transferRingDescriptor.pcs;
   tempControlEndpoint.interval = 0;
   tempControlEndpoint.maxPrimaryStreams = 0;
   tempControlEndpoint.mult = 0;
   tempControlEndpoint.errorCount = 3;
   *controlEndpoint = tempControlEndpoint;

   memset((void*)&outputContext[slotId], 0, sizeof(XhcOutputContext));


   dcBaseAddressArray[slotId] =  (uint64_t)&outputContext[slotId];

   xhcd_putTRB(TRB_ADDRESS_DEVICE((uint64_t)&inputContext[slotId], slotId, 0), &commandRingDescriptor);
   ringCommandDoorbell(xhci);
   XhcEventTRB result;
   while(xhcd_readEvent(&eventRingDescriptor, &result, 1) == 0);
   if(result.completionCode != Success){
      printf("[xhc] failed to addres device (Event: %X %X %X %X, code: %d)\n", result, result.completionCode);
      if(result.completionCode == TrbError){
         printf("(trb error)\n");

      }
      return 0;
   }
   printf("[xhc] successfully addressed device: (Event: %X %X %X %X)\n", result);
   while(1);
   
   return 1;
}
static void test(Xhci *xhci){

   xhcd_putTRB(TRB_NOOP(), &commandRingDescriptor);
   ringCommandDoorbell(xhci);
   uint32_t status = xhci->operation->USBStatus;
   printf("Error? %d\n", status & (1 << 12));

   XhcEventTRB result;
   printf("Waiting for interruptor\n");
   while(!xhcd_readEvent(&eventRingDescriptor, &result, 1));
   printf("event posted %X %X %X %X\n", result);
   while(1);
}
/*
static int shouldEnablePort (Xhci *xhci, int portNumber){
   PortStatusAndControll *status = getPortStatus(xhci, portNumber);
   printf("portLinkState %d\n", status->portLinkState);
   if(status->portLinkState == LINK_STATE_RX_DETECT){
      return 0;
   }
   return status->portLinkState == LINK_STATE_POLLING;
}
*/
int xhcd_isPortEnabled(Xhci *xhci, int portIndex){
      PortStatusAndControll status = *getPortStatus(xhci, portIndex);
      if(status.portEnabledDisabled == 1
            && status.portReset == 0
            && status.portLinkState == 0){
         return 1;
      }
      return 0;
}
static int getSlotType(Xhci *xhci){
   XhciCapabilities *cap = xhci->capabilities;
   uint32_t base = (uint32_t)cap;
   CapabilityParams1 xcap = cap->capabilityParams1;
   uint32_t offset = (uint32_t)xcap.extendedCapabilitiesPointer << 2;
   XhciExtendedCapabilities *xhc = (XhciExtendedCapabilities*)(base + offset);

   while(xhc->capabilityId != EXTENDED_CAPABILITY_SUPPORTED_PROTOCOL){
      if(xhc->nextExtendedCapabilityPointer == 0){
         return -1;
      }
      xhc += xhc->nextExtendedCapabilityPointer << 2;
   }
   uint8_t slotType = xhc->body[2] & 0xF;
   return slotType;
}
static void ringCommandDoorbell(Xhci *xhci){
   volatile uint32_t *doorbell = (uint32_t *)xhci->doorbells;
   doorbell[0] = 0;
}

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portIndex){
   XhciOperation *operation = xhci->operation;
   XhciPortRegisters *port = &operation->ports[portIndex];
   PortStatusAndControll *status = &port->statusAndControll;
   return status;
}
static int initBasePointers(PciGeneralDeviceHeader *pciHeader, Xhci *xhci){
   if(pciHeader->baseAddress1 != 0){
      printf("Error: unable to reach xhcd MMIO in 32 bit mode: %X %X\n",
            pciHeader->baseAddress0, pciHeader->baseAddress1);
      return -1;
   }

   uint32_t base = pciHeader->baseAddress0 & (~0b111111); //FIXME: what is this?

   
   xhci->capabilities = (XhciCapabilities *)base;
   uint32_t capLength = xhci->capabilities->capabilityRegistersLength;
   uint32_t doorbellOffset = xhci->capabilities->doorbellOffset;
   uint32_t runtimeRegOffset = xhci->capabilities->runtimeRegisterSpaceOffset;

   xhci->operation = (XhciOperation *) (base + capLength);
   xhci->doorbells = (XhciDoorbell *) (base + doorbellOffset);
   xhci->interrupterRegisters = (InterrupterRegisters *)(base + runtimeRegOffset + 0x20);

   printf("addressed cap:%X door:%X, interrupt:%X op: %X\n", xhci->capabilities, xhci->doorbells, xhci->interrupterRegisters, xhci->operation);

   return 0;
}
static void waitForControllerReady(Xhci *xhci){
   printf("Waiting for xhci controller ready...\n");
   volatile uint32_t *us = &xhci->operation->USBStatus;
   uint32_t usbStatus = *us;
   while(usbStatus & CNR_FLAG){
      usbStatus = *us;
   }
}
static void setMaxEnabledDeviceSlots(Xhci *xhci){
   volatile uint32_t *configure = (uint32_t*)&xhci->operation->configure;
   uint32_t val = *configure;
   val &=  ~0xFF;
   val |= MAX_DEVICE_SLOTS_ENABLED;
   *configure = val;
}
static void initDCAddressArray(Xhci *xhci){
   memset((void*)dcBaseAddressArray, 0, sizeof(dcBaseAddressArray));
   uint32_t addr = (uint32_t)dcBaseAddressArray; 
   xhci->operation->dcAddressArrayPointer = (uint64_t)addr;
}

static void initScratchPad(Xhci *xhci){
   StructParams2 structParams2 = xhci->capabilities->structParams2;
   uint32_t scratchpadSize = structParams2.maxScratchpadBuffersHigh << 5;
   scratchpadSize |= structParams2.maxScratchpadBuffersLow;
   printf("required scratchpadSize %d\n", scratchpadSize);

   //FIXME: this could fuck the memory, you have to implement malloc etc.
   uint32_t pageSize = xhci->operation->pageSize << 12;
   printf("pageSize: %d\n", pageSize);
   uint32_t currPagePoss = (uint32_t)scratchpadStart % pageSize;
   uint32_t offset = pageSize - currPagePoss;
   if(offset == pageSize){
      offset = 0;
   }
   uint64_t *scratchpad = (uint64_t*)((uint32_t)scratchpadStart + offset);
   printf("scratchpad begin at %X\n",scratchpad);
   if((uint32_t)scratchpad % pageSize != 0){
      printf("scratchpad pos invalid %d\n", (uint32_t)scratchpad % pageSize);
   }
   for(uint32_t i = 0; i < scratchpadSize; i++){
      scratchpadPointers[i] = (uint64_t)&scratchpad[i];
      memset(&scratchpad[i], 0, pageSize);
   }

   dcBaseAddressArray[0] = (uint64_t)scratchpadPointers;
}
static void turnOnController(Xhci *xhci){
   uint32_t command = xhci->operation->USBCommand;
   command |= USBCMD_RUN_STOP_BIT;
   xhci->operation->USBCommand = command;
}
