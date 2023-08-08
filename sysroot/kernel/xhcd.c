#include "kernel/xhcd.h"
#include "stdio.h"
#include "stdlib.h"

//DC = Device context
//p.168. TRB rings shall not cross 64KB boundary
//p82 UBS LS FS HS requres port process to advance to enabled state

#define CNR_FLAG (1<<11)

#define MAX_DEVICE_SLOTS_ENABLED 16
#define DEFAULT_COMMAND_RING_SIZE 16

#define USBCMD_RUN_STOP_BIT 1

#define LINK_STATE_RX_DETECT 5
#define LINK_STATE_POLLING 7

#define TRB_ENABLE_SLOT_COMMAND 9
#define TRB_NO_OP_COMMAND 23
#define TRB_LINK 6

#define EXTENDED_CAPABILITY_SUPPORTED_PROTOCOL 2

#define DEFAULT_PCS 1

#define DEFAULT_EVENT_SEGEMNT_TRB_COUNT 16

static int initBasePointers(PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
static void waitForControllerReady(Xhci *xhci);
static void setMaxEnabledDeviceSlots(Xhci *xhci);
static void initDCAddressArray(Xhci *xhci);
static void initCommandRing(Xhci *xhci);
static void turnOnController(Xhci *xhci);
static void initEventRing(Xhci *xhci);
static void initScratchPad(Xhci *xhci);

static void putCommand(TRB *command);
static int putEnableSlotCommand(Xhci *xchi);
static int getSlotType(Xhci *xhci);

static void test(Xhci *xhci);

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portNumber);
//static int shouldEnablePort(Xhci *xhci, int portNumber);

__attribute__((aligned(64)))
static volatile uint64_t dcBaseAddressArray[MAX_DEVICE_SLOTS_ENABLED + 1];

__attribute((aligned(64)))
static volatile TRB commandRing[DEFAULT_COMMAND_RING_SIZE];

__attribute__((aligned(64)))
static volatile EventRingSegmentTableEntry eventRingSegmentTable[1];

__attribute__((aligned(64)))
static volatile CommandCompletionEventTRB eventRing[DEFAULT_EVENT_SEGEMNT_TRB_COUNT];

static volatile uint64_t scratchpadPointers[255];

__attribute__((aligned(4096)))
static volatile uint8_t scratchpadStart[139264];


static int commandRingIndex = 0;

int xhcd_init(PciGeneralDeviceHeader *pciHeader, Xhci *xhci){
   int errorCode = 0;
   if((errorCode = initBasePointers(pciHeader, xhci)) != 0){
      return errorCode;
   }
   printf("Waiting for xhci controller ready...\n");
   waitForControllerReady(xhci);
   setMaxEnabledDeviceSlots(xhci);
   initDCAddressArray(xhci);
   initCommandRing(xhci);
   initEventRing(xhci);
   initScratchPad(xhci);
   turnOnController(xhci);
   printf("xhc turned on\n");
   printf("xhc running %b\n", xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT);
   printf("xhc halted %b\n", xhci->operation->USBStatus & 1);
   printf("xhc running %b\n", xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT);
   while(xhci->operation->USBStatus & 1);

   printf("Error? %d\n", xhci->operation->USBStatus & (1 << 12));

   test(xhci);

   return 0;
}
int xhcd_checkForDeviceAttach(Xhci *xhci){  
   StructParams1 structParams1 = xhci->capabilities->structParams1;
   int count = structParams1.maxPorts;
   XhciOperation *operation = xhci->operation;
   for(int i = 0; i < count; i++){
      XhciPortRegisters *port = &operation->ports[i];
      PortStatusAndControll status = port->statusAndControll;
      if(status.connectStatusChange && status.currentConnectStatus){
         status.connectStatusChange = 1;
            *(&port->statusAndControll) = status;
         return i;
      }
   }
   return -1;
}
int xhcd_enable(Xhci *xhci, int portNumber){
   if(xhcd_isPortEnabled(xhci, portNumber)){
      printf("Port already enabled (USB3)\n");
      return 1;
   }
/*   if(!shouldEnablePort(xhci, portNumber)){ //FIXME: This should worrk
      printf("Failed to enable port (USB3)\n");
      return 0;
   }*/
   printf("Enabling port (USB2)\n");
   PortStatusAndControll *statusAndControll = getPortStatus(xhci, portNumber);
   PortStatusAndControll temp = *statusAndControll;
   temp.portReset = 1;
   *statusAndControll = temp;
   while(!temp.portResetChange){
      temp = *statusAndControll;
   }
   temp.portResetChange = 1; //Clear
   *statusAndControll = temp; 
   int isEnabled = xhcd_isPortEnabled(xhci, portNumber);
   if(!isEnabled){
      return 0;
   }

   return 1;
}
static void test(Xhci *xhci){

   volatile uint32_t *doorbell = (uint32_t *)xhci->doorbells;

//   InterrupterRegisters *interruptor = &xhci->interrupterRegisters[0];
//   interruptor->interruptPending = 1;
//   printf("Interruptor state: %d\n", interruptor->interruptPending);

   putEnableSlotCommand(xhci);
   for(int i = 0; i < DEFAULT_COMMAND_RING_SIZE; i++){
  //    printf("%X %X %X %X,  ", commandRing[i]);
   }
   printf("\n");
   doorbell[0] = 0;
   for(int i = 0; i < 2; i++){
      uint32_t running = xhci->operation->commandRingControllLow;
      printf("command ring running: %d\n", running & (1 << 3));
   }
   uint32_t status = xhci->operation->USBStatus;
   printf("Error? %d\n", status & (1 << 12));

   printf("Waiting for interruptor\n");
   CommandCompletionEventTRB topEvent = eventRing[0];
   while(topEvent.completionCode == 0){
      topEvent = eventRing[0];
      printf("waiting... %X %X %X %X\n", topEvent);
   }
   printf("event posted\n");
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
int xhcd_isPortEnabled(Xhci *xhci, int portNumber){
      PortStatusAndControll status = *getPortStatus(xhci, portNumber);
      if(status.portEnabledDisabled == 1
            && status.portReset == 0
            && status.portLinkState == 0){
         return 1;
      }
      return 0;
}
static int putEnableSlotCommand(Xhci *xhci){
   TRB command = {0,0,0,0};
   uint32_t trbType = TRB_NO_OP_COMMAND;
  /* int8_t slotType = getSlotType(xhci);
   if(slotType == -1){
      printf("Unable to find xhci slot type\n");
      return 0;
   }
   printf("Slot type %d\n", slotType);*/
   uint32_t slotType = 0;
   command.r3 = (trbType << 10) | (slotType << 16) | DEFAULT_PCS;
   putCommand(&command);
   return 1;
}
static int getSlotType(Xhci *xhci){
   XhciCapabilities *cap = xhci->capabilities;
   uint32_t base = (uint32_t)cap;
   uint32_t offset = (uint32_t)cap->capabilityParams1.extendedCapabilitiesPointer << 2;
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
static void putCommand(TRB *command){
   memcpy((void*)&commandRing[0], (void*)command, sizeof(TRB));
   printf("command: %X %X %X %X\n", commandRing[0]);
   commandRingIndex++;
   if(commandRingIndex > (int)(sizeof(commandRing) / sizeof(TRB))){
      commandRingIndex = 0;
   }
}

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portNumber){
   XhciOperation *operation = xhci->operation;
   XhciPortRegisters *port = &operation->ports[portNumber];
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
   volatile uint32_t *us = &xhci->operation->USBStatus;
   uint32_t usbStatus = *us;
   while(usbStatus & CNR_FLAG){
      usbStatus = *us;
   }
}
static void setMaxEnabledDeviceSlots(Xhci *xhci){
   uint32_t *configure = (uint32_t*)&xhci->operation->configure;
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
static void initCommandRing(Xhci *xhci){
   commandRingIndex = 0;
   memset((void*)commandRing, 0, sizeof(commandRing));
   for(int i = 0; i < DEFAULT_COMMAND_RING_SIZE; i++){
      commandRing[i].r3 |= !DEFAULT_PCS;
   }

   uint32_t addr = (uint32_t)&commandRing[0];
   printf("Commandring at %X\n", addr);
   uint32_t low = addr | DEFAULT_PCS;
   xhci->operation->commandRingControllLow = low;
   xhci->operation->commandRingControllHigh = 0;
//   uint32_t *crch = &xhci->operation->commandRingControllHigh;
  // *crch = 0x69;

   LinkTRB link;
   memset((void*)&link, 0, sizeof(LinkTRB));
   link.ringSegmentLow = addr;
   link.cycleBit = DEFAULT_PCS;
   link.toggleCycle = 1;
   link.trbType = TRB_LINK;
   memcpy((void*)&commandRing[DEFAULT_COMMAND_RING_SIZE - 1],
         (void*)&link,
         sizeof(LinkTRB));
}
static void initEventRing(Xhci *xhci){
   memset((void*)eventRing, 0, sizeof(eventRing));
   EventRingSegmentTableEntry *table = &eventRingSegmentTable[0];
   EventRingSegmentTableEntry tempTable = *table;
   tempTable.baseAddressLow = (uint32_t)eventRing;
   tempTable.baseAddressHigh = 0;
   tempTable.ringSegmentSize = DEFAULT_EVENT_SEGEMNT_TRB_COUNT;
   *table = tempTable;

   InterrupterRegisters * interrupter = &(xhci->interrupterRegisters[0]);

   printf("eventdisc %X\n", eventRingSegmentTable);
   interrupter->eventRingSegmentTableSize = 1;
   interrupter->eventRingDequePointerHigh = 0;
   interrupter->eventRingDequePointerLow = (uint32_t)eventRing;
   interrupter->eventRingSegmentTableAddress = (uint64_t)eventRingSegmentTable;


   printf("intsizew %X\n", sizeof(InterrupterRegisters));
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
