#include "kernel/xhcd.h"
#include "kernel/xhcd-ring.h"
#include "kernel/xhcd-event-ring.h"
#include "kernel/usb-descriptors.h"
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

#define ENDPOINT_TYPE_CONTROL 4
#define ENDPOINT_TYPE_INTERRUPT_IN 7
#define ENDPOINT_TYPE_INTERRUPT_OUT 3

#define INPUT_CONTEXT_A0A1_MASK 0b11

#define DESCRIPTOR_TYPE_DEVICE 1
#define DESCRIPTOR_TYPE_CONFIGURATION 2
#define DESCRIPTOR_TYPE_INTERFACE 4
#define DESCRIPTOR_TYPE_ENDPOINT 5

static int initBasePointers(const PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
static void waitForControllerReady(Xhci *xhci);
static void setMaxEnabledDeviceSlots(Xhci *xhci);
static void resetXhc(Xhci *xhci);
static void initCommandRing(Xhci *xhci);
static void initEventRing(Xhci *xhci);
static void initDCAddressArray(Xhci *xhci);
static void turnOnController(Xhci *xhci);
static void initScratchPad(Xhci *xhci);
static int enablePort(Xhci *xhci, int portIndex);
static int isPortEnabled(Xhci *xhci, int portIndex);
static int checkoutPort(Xhci *xhci, int portIndex);

static int getSlotId(Xhci *xhci);
static int addressDevice(Xhci *xhci, int slotId, int portIndex);
static void initDefaultInputContext(XhcInputContext *inputContext, int portIndex, XhcdRing transferRing);

static XhcStatus configureEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint);
static XhcStatus runConfigureEndpointCommand(Xhci *xhci, int slotId, int endpointIndex, XhcEndpointContext *endpointContext);

static XhcStatus initInterruptEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint);
static XhcStatus initBulkEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint);

static int getEndpointIndex(UsbEndpointDescriptor *endpoint);

static XhcStatus initDevice(Xhci *xhci, int portIndex, XhcDevice *result);
static int getNewlyAttachedDevices(Xhci *xhci, uint32_t *result, int bufferSize);
static int setMaxPacketSize(Xhci *xhci, int slotId);

static int getSlotType(Xhci *xhci);
static void ringCommandDoorbell(Xhci *xhci);

static void test(Xhci *xhci);
static int putConfigTD(Xhci *xhci, int slotId, TD td);
static void xhcd_ringDoorbell(Xhci *xhci, uint8_t slotId, uint8_t target);
static XhcOutputContext *getOutputContext(Xhci *xhci, int slotId);

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portNumber);
//static int shouldEnablePort(Xhci *xhci, int portNumber);

__attribute__((aligned(64)))
static XhcInputContext inputContext[MAX_DEVICE_SLOTS_ENABLED];

XhcStatus xhcd_init(const PciGeneralDeviceHeader *pciHeader, Xhci *xhci){
   int errorCode = 0;
   if((errorCode = initBasePointers(pciHeader, xhci)) != 0){
      return errorCode;
   }

   resetXhc(xhci);
   waitForControllerReady(xhci);
   setMaxEnabledDeviceSlots(xhci);
   initDCAddressArray(xhci);
   initCommandRing(xhci);
   initEventRing(xhci);
   initScratchPad(xhci);
   turnOnController(xhci);

   printf("xhc running %b\n", xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT);
   printf("xhc halted %b\n", xhci->operation->USBStatus & 1);
   printf("Error? %d\n", xhci->operation->USBStatus & (1 << 12));
   
   //FIXME: a bit of hack, clearing event ring
   XhcEventTRB result[16];
   while(xhcd_readEvent(&xhci->eventRing, result, 16));

   return XhcOk;
}

int xhcd_getDevices(Xhci *xhci, XhcDevice *resultBuffer, int bufferSize){
   uint32_t *portIndexes = malloc(bufferSize * sizeof(uint32_t));
   int count = getNewlyAttachedDevices(xhci, portIndexes, bufferSize);
   for(int i = 0; i < count; i++){
      XhcStatus status = initDevice(xhci, portIndexes[i], &resultBuffer[i]);
      if(status != XhcOk){
         i--;
         count--;
         //FIXME: Some kind of error message here?
      }
   }
   free(portIndexes);
   return count;
}
static int getNewlyAttachedDevices(Xhci *xhci, uint32_t *result, int bufferSize){
   StructParams1 structParams1 = xhci->capabilities->structParams1;
   int count = structParams1.maxPorts;

   int resultIndex = 0;
   for(int i = 0; i < count && resultIndex < bufferSize; i++){
      if(checkoutPort(xhci, i)){
         result[resultIndex] = i;
         resultIndex++;
      }
      
   }
   return resultIndex;
}
static XhcStatus initDevice(Xhci *xhci, int portIndex, XhcDevice *result){
   if(!enablePort(xhci, portIndex)){
      return XhcEnablePortError;
   }

   int slotId = getSlotId(xhci);
   if(slotId < 0){
      return XhcSlotIdError;
   }

   if(!addressDevice(xhci, slotId, portIndex)){
      return XhcAddressDeviceError;
   }

   if(!setMaxPacketSize(xhci, slotId)){
      return XhcSetMaxPacketSizeError;
   }
   *result = (XhcDevice){slotId, xhci};
   return XhcOk;
}
XhcStatus xhcd_setConfiguration(XhcDevice *device, const UsbConfiguration *configuration){
   for(int i = 0; i < configuration->descriptor.bNumInterfaces; i++){
      UsbInterface *interface = &configuration->interfaces[i];
      for(int j = 0; j < interface->descriptor.bNumEndpoints; j++){
         UsbEndpointDescriptor *endpointDescriptor = &interface->endpoints[j];
         int status = configureEndpoint(device->xhci, device->slotId, endpointDescriptor);
         if(status != XhcOk){
            return status;
         }
      }
   }
   return XhcOk;
}
static XhcStatus configureEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint){
   switch(endpoint->transferType){
      case ENDPOINT_TRANSFER_TYPE_INTERRUPT:
         return initInterruptEndpoint(xhci, slotId, endpoint);
      case ENDPOINT_TRANSFER_TYPE_BULK:
         return initBulkEndpoint(xhci, slotId, endpoint);
      default:
         printf("Transfer type not yet implemented %d\n", endpoint->transferType);
         return XhcNotYetImplemented;
   }
}
XhcStatus xhcd_readData(const XhcDevice *device, int endpoint, void *dataBuffer, uint16_t bufferSize){
   Xhci *xhci = device->xhci;
   TRB trb = TRB_NORMAL(dataBuffer, bufferSize);
   XhcdRing *transferRing = &xhci->transferRing[device->slotId][endpoint - 1];
   xhcd_putTRB(trb, transferRing);
   xhcd_ringDoorbell(xhci, device->slotId, endpoint);

   XhcEventTRB event;
   while(!xhcd_readEvent(&xhci->eventRing, &event, 1));
   if(event.completionCode != Success){
      return XhcReadDataError;
   }
   return XhcOk;
}
XhcStatus xhcd_sendRequest(const XhcDevice *device, UsbRequestMessage request){
   SetupStageHeader header;
   header.bmRequestType = request.bmRequestType;
   header.bRequest = request.bRequest;
   header.wValue = request.wValue;
   header.wIndex = request.wIndex;
   header.wLength = request.wLength;
   TRB setupTrb = TRB_SETUP_STAGE(header);
   TRB statusTrb = TRB_STATUS_STAGE();

   TD td;
   if(header.wLength == 0){
      td = (TD){{setupTrb, statusTrb}, 2};
   }else{
      TRB dataTrb = TRB_DATA_STAGE((uintptr_t)request.dataBuffer, request.wLength);
      td = (TD){{setupTrb, dataTrb, statusTrb}, 3};
   }
   if(!putConfigTD(device->xhci, device->slotId, td)){
      return XhcSendRequestError;

   }
   return XhcOk;
}
static XhcStatus initInterruptEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint){
   int endpointIndex = getEndpointIndex(endpoint);

   XhcdRing transferRing = xhcd_newRing(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhci->transferRing[slotId][endpointIndex - 1] = transferRing;

   uint32_t maxPacketSize = endpoint->wMaxPacketSize & 0x7FF;
   uint32_t maxBurstSize = (endpoint->wMaxPacketSize & 0x1800) >> 11;
   uint32_t maxESITPayload = maxPacketSize * (maxBurstSize + 1);
   uint32_t endpointType =
      endpoint->direction == ENDPOINT_DIRECTION_IN ? ENDPOINT_TYPE_INTERRUPT_IN : ENDPOINT_TYPE_INTERRUPT_OUT;

   XhcEndpointContext endpointContext = (XhcEndpointContext){
      .endpointType = endpointType,
      .maxPacketSize = maxPacketSize,
      .maxBurstSize = maxBurstSize,
      .mult = 0,
      .errorCount = 3,
      .dequeuePointer = (uintptr_t)transferRing.dequeue | transferRing.pcs,
      .maxESITPayloadLow = (uint16_t)maxESITPayload, 
      .maxESITPayloadHigh = maxESITPayload >> 16,
      .interval = endpoint->bInterval,
   };
   return runConfigureEndpointCommand(xhci, slotId, endpointIndex, &endpointContext);
}
static XhcStatus initBulkEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint){
   int endpointIndex = getEndpointIndex(endpoint);

   XhcdRing transferRing = xhcd_newRing(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhci->transferRing[slotId][endpointIndex - 1] = transferRing;

   XhcEndpointContext endpointContext = (XhcEndpointContext)
   {
      .endpointType = endpoint->direction == ENDPOINT_DIRECTION_IN ? 0 : 0, //TBD
      .maxPacketSize = endpoint->wMaxPacketSize,
      .maxBurstSize = 0, //TBD
      .errorCount = 3,
      //TBD
   };
   return runConfigureEndpointCommand(xhci, slotId, endpointIndex, &endpointContext);
}

static int getSlotId(Xhci *xhci){
   xhcd_putTRB(TRB_ENABLE_SLOT(getSlotType(xhci)), &xhci->commandRing);
   ringCommandDoorbell(xhci);
   XhcEventTRB trb;
   //FIXME: hack
   while(!xhcd_readEvent(&xhci->eventRing, &trb, 1) || trb.trbType == PortStatusChangeEvent);
   if(trb.trbType != CommandCompletionEvent){
      printf("[xhc] unknown event %X %X %X %X (event %d)\n", trb, trb.trbType);
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
   return trb.slotId;
}
static void initDefaultInputContext(XhcInputContext *inputContext, int portIndex, XhcdRing transferRing){
   memset((void*)inputContext, 0, sizeof(XhcInputContext));
   inputContext->inputControlContext.addContextFlags |= INPUT_CONTEXT_A0A1_MASK;
   
   XhcSlotContext *slotContext = &inputContext->slotContext;
   XhcSlotContext tempSlotContext = *slotContext;
   tempSlotContext.rootHubPortNumber = portIndex + 1; //port number is 1 indexed
   tempSlotContext.routeString = 0;
   tempSlotContext.contextEntries = 1;
   *slotContext = tempSlotContext;

   XhcEndpointContext *controlEndpoint = &inputContext->endpointContext[0];
   XhcEndpointContext tempControlEndpoint = *controlEndpoint;
   tempControlEndpoint.endpointType = ENDPOINT_TYPE_CONTROL;
   tempControlEndpoint.maxPacketSize = 8; //FIXME: what value?
   tempControlEndpoint.maxBurstSize = 0;
   tempControlEndpoint.dequeuePointer = (uintptr_t)transferRing.dequeue | transferRing.pcs;
   tempControlEndpoint.interval = 0;
   tempControlEndpoint.maxPrimaryStreams = 0;
   tempControlEndpoint.mult = 0;
   tempControlEndpoint.errorCount = 3;
   *controlEndpoint = tempControlEndpoint;

}
static int addressDevice(Xhci *xhci, int slotId, int portIndex){
   XhcOutputContext *outputContext = callocco(sizeof(XhcOutputContext), 64, 0);
   xhci->dcBaseAddressArray[slotId] = (uintptr_t)outputContext;

   XhcdRing transferRing = xhcd_newRing(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhci->transferRing[slotId][0] = transferRing;
   
   initDefaultInputContext(&inputContext[slotId], portIndex, transferRing);
   xhcd_putTRB(TRB_ADDRESS_DEVICE((uintptr_t)&inputContext[slotId], slotId, 0), &xhci->commandRing);
   ringCommandDoorbell(xhci);
   XhcEventTRB result;
   while(xhcd_readEvent(&xhci->eventRing, &result, 1) == 0);
   if(result.completionCode != Success){
      printf("[xhc] failed to addres device (Event: %X %X %X %X, code: %d)\n", result, result.completionCode);
      return 0;
   }
   printf("[xhc] successfully addressed device: (Event: %X %X %X %X)\n", result);
   return 1;
}
static int checkoutPort(Xhci *xhci, int portIndex){
   XhciOperation *operation = xhci->operation;
   XhciPortRegisters *port = &operation->ports[portIndex];
   PortStatusAndControll status = port->statusAndControll;
   if(status.connectStatusChange && status.currentConnectStatus){
      status.connectStatusChange = 1;
      port->statusAndControll = status;
      return 1;
   }   
   return 0;

}
static void resetXhc(Xhci *xhci){
   xhci->operation->USBCommand &= ~1;
   while(!(xhci->operation->USBStatus & 1));
   xhci->operation->USBCommand |= 1 << 1;
}
static void initCommandRing(Xhci *xhci){
   xhci->commandRing = xhcd_newRing(DEFAULT_COMMAND_RING_SIZE);
   xhcd_attachCommandRing(xhci->operation, &xhci->commandRing);
}
static void initEventRing(Xhci *xhci){
   xhci->eventRing = xhcd_newEventRing(DEFAULT_EVENT_SEGEMNT_TRB_COUNT);
   xhcd_attachEventRing(&xhci->eventRing, &xhci->interrupterRegisters[0]);
}
static int enablePort(Xhci *xhci, int portIndex){
   if(isPortEnabled(xhci, portIndex)){
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
   while(!temp.portEnabledDisabled){
      temp = *statusAndControll;
   }
   printf("status: %X\n", *statusAndControll);
   int isEnabled = isPortEnabled(xhci, portIndex);
   temp.portResetChange = 1; //Clear FIXME: this acts strange
   *statusAndControll = temp; 
   printf("old status: %X\n", *statusAndControll);
   if(!isEnabled){
      return 0;
   }

   return 1;
}
static int runCommand(Xhci *xhci, TRB trb){
   xhcd_putTRB(trb, &xhci->commandRing);
   ringCommandDoorbell(xhci);

   XhcEventTRB result;
   while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
   if(result.completionCode != Success){
      return 0;
   }
   return 1;
}
static XhcStatus runConfigureEndpointCommand(Xhci *xhci, int slotId, int endpointIndex, XhcEndpointContext *endpointContext){
   XhcOutputContext *output = getOutputContext(xhci, slotId);
   XhcInputContext input;
   memset((void*)&input, 0, sizeof(XhcInputContext));

   input.inputControlContext.addContextFlags |= (1 << endpointIndex) | 1;
   //controll->configurationValue = config.configurationValue;
   input.slotContext = output->slotContext;
   input.slotContext.contextEntries = endpointIndex;
   input.endpointContext[endpointIndex - 1] = *endpointContext;

   TRB trb = TRB_CONFIGURE_ENDPOINT((void*)&input, slotId);
   if(!runCommand(xhci, trb)){
      printf("[xhc] failed to configure endpoint (slotid: %d)\n", slotId);
      return XhcConfigEndpointError;
   }
   return XhcOk;
}
static int getEndpointIndex(UsbEndpointDescriptor *endpoint){
   int index = endpoint->endpointNumber * 2;
   if(endpoint->direction == ENDPOINT_DIRECTION_IN){
      index += 1;
   }
   return index;
}
static XhcOutputContext *getOutputContext(Xhci *xhci, int slotId){
   volatile uint64_t *dcAddressArray = xhci->dcBaseAddressArray;
   uintptr_t ptr = dcAddressArray[slotId];
   XhcOutputContext *outputContext = (XhcOutputContext*)ptr;
   return outputContext;
}

static int putConfigTD(Xhci *xhci, int slotId, TD td){
   XhcdRing *transferRing = &xhci->transferRing[slotId][0];
   xhcd_putTD(td, transferRing);
   xhcd_ringDoorbell(xhci, slotId, 1);

   XhcEventTRB event;
   while(!xhcd_readEvent(&xhci->eventRing, &event, 1));
   if(event.completionCode != Success){
      return 0;
   }
   return 1;
}
static int setMaxPacketSize(Xhci *xhci, int slotId){
   uint8_t buffer[8];
   XhcdRing *transferRing = &xhci->transferRing[slotId][0];
   xhcd_putTD(TD_GET_DESCRIPTOR(buffer, sizeof(buffer)), transferRing);
   xhcd_ringDoorbell(xhci, slotId, 1);

   XhcEventTRB result;
   while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
   if(result.completionCode != Success){
      printf("[xhc] failed to get max packet size\n");
      return 0;
   }
   uint8_t maxPacketSize = buffer[7];

   uintptr_t address = xhci->dcBaseAddressArray[slotId];
   XhcOutputContext *output = (XhcOutputContext*)address;
   uint8_t currMaxPacketSize = output->endpointContext[0].maxPacketSize;

   if(maxPacketSize != currMaxPacketSize){
      XhcInputContext *input = &inputContext[slotId];
      input->endpointContext[0] = output->endpointContext[0];
      input->endpointContext[0].maxPacketSize = maxPacketSize;
      memset((void*)&input->inputControlContext, 0, sizeof(XhcInputControlContext));
      input->inputControlContext.addContextFlags = 1 << 1;
      printf("add context %X\n", input->inputControlContext.addContextFlags);
      xhcd_putTRB(TRB_EVALUATE_CONTEXT((void*)input, slotId), &xhci->commandRing);
      ringCommandDoorbell(xhci);
      XhcEventTRB result;
      while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
      if(result.completionCode != Success){
         printf("[xhc] failed to set max packet size\n");
         return 0;
      }
   }
   currMaxPacketSize = output->endpointContext[0].maxPacketSize;
   printf("[xhc] sucessfully set max packet size: %d\n", currMaxPacketSize);
   return 1;
}
static void test(Xhci *xhci){
   xhcd_putTRB(TRB_NOOP(), &xhci->commandRing);
   ringCommandDoorbell(xhci);
   for(int i = 0; i < 2; i++){
      uint32_t status = xhci->operation->USBStatus;
      printf("Error? %d\n", status & (1 << 12));
   }

   XhcEventTRB result;
   printf("Waiting for interruptor\n");
   while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
   printf("event posted %X %X %X %X\n", result);
   printf("completion code: %d (success: %b)\n", result.completionCode, result.completionCode == Success);
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
static int isPortEnabled(Xhci *xhci, int portIndex){
      PortStatusAndControll status = *getPortStatus(xhci, portIndex);
      printf("enabled? %b %b %b\n", status.portEnabledDisabled, !status.portReset, !status.portLinkState);
      printf("new status %X\n", status);
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
static void xhcd_ringDoorbell(Xhci *xhci, uint8_t slotId, uint8_t target){
   if(slotId == 0){
      printf("[xhc] Unable to ring doorbell. Invalid slotId: 0\n");
      return;
   }
   if(target < 1){
      printf("[xhc] Unable to ring doorbell. Invalid target: 0\n");
      return;
   }
   volatile uint32_t *doorbell = (uint32_t*)xhci->doorbells;
   doorbell[slotId] = target; 
}

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portIndex){
   XhciOperation *operation = xhci->operation;
   XhciPortRegisters *port = &operation->ports[portIndex];
   PortStatusAndControll *status = &port->statusAndControll;
   return status;
}
static int initBasePointers(const PciGeneralDeviceHeader *pciHeader, Xhci *xhci){
   if(pciHeader->baseAddress1 != 0){
      printf("Error: unable to reach xhcd MMIO in 32 bit mode: %X %X\n",
            pciHeader->baseAddress0, pciHeader->baseAddress1);
      return -1;
   }

   uint32_t base = pciHeader->baseAddress0 & (~15); //FIXME: what is this?

   
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
   while(xhci->operation->USBStatus & CNR_FLAG);
}
static void setMaxEnabledDeviceSlots(Xhci *xhci){
   volatile uint32_t *configure = (uint32_t*)&xhci->operation->configure;
   uint32_t val = *configure;
   val &=  ~0xFF;
   val |= MAX_DEVICE_SLOTS_ENABLED;
   *configure = val;
}
static void initDCAddressArray(Xhci *xhci){
   xhci->dcBaseAddressArray = callocco((MAX_DEVICE_SLOTS_ENABLED + 1) * sizeof(uint64_t), 64, 0);
   printf("dc array at %X\n", xhci->dcBaseAddressArray);
   uintptr_t addr = (uintptr_t)xhci->dcBaseAddressArray; 
   xhci->operation->dcAddressArrayPointer = addr;
}

static void initScratchPad(Xhci *xhci){
   StructParams2 structParams2 = xhci->capabilities->structParams2;
   uint32_t scratchpadSize = structParams2.maxScratchpadBuffersHigh << 5;
   scratchpadSize |= structParams2.maxScratchpadBuffersLow;
   printf("required scratchpadSize %d\n", scratchpadSize);
   /*if(scratchpadSize == 0){
      return;
   }*/

   uint32_t pageSize = xhci->operation->pageSize << 12;
   printf("pageSize: %d\n", pageSize);
   uint64_t *scratchpadPointers = mallocco(scratchpadSize * sizeof(uint64_t), 64, pageSize);

   for(uint32_t i = 0; i < scratchpadSize; i++){
      void* scratchpadStart = callocco(pageSize, 1, pageSize);
      scratchpadPointers[i] = (uintptr_t)scratchpadStart;
   }
   xhci->dcBaseAddressArray[0] = (uintptr_t)scratchpadPointers;
   printf("[xhc] initialized scratchpad (%X)\n", scratchpadPointers);
}
static void turnOnController(Xhci *xhci){
   uint32_t command = xhci->operation->USBCommand;
   command |= USBCMD_RUN_STOP_BIT;
   xhci->operation->USBCommand = command;
}
