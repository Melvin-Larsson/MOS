#include "kernel/xhcd.h"
#include "kernel/xhcd-ring.h"
#include "kernel/xhcd-event-ring.h"
#include "kernel/usb-descriptors.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "kernel/interrupt.h"


//FIXME: remove
#include "kernel/pci.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"
//DC = Device context
//p.168. TRB rings shall not cross 64KB boundary
//p82 UBS LS FS HS requres port process to advance to enabled state


#define CNR_FLAG (1<<11)

#define MAX_DEVICE_SLOTS_ENABLED 16
#define DEFAULT_COMMAND_RING_SIZE 32
#define DEFAULT_EVENT_SEGEMNT_TRB_COUNT 32
#define DEFAULT_TRANSFER_RING_TRB_COUNT 16

#define USBCMD_RUN_STOP_BIT 1

#define LINK_STATE_RX_DETECT 5
#define LINK_STATE_POLLING 7

#define EXTENDED_CAPABILITY_SUPPORTED_PROTOCOL 2

#define ENDPOINT_TYPE_CONTROL 4
#define ENDPOINT_TYPE_INTERRUPT_IN 7
#define ENDPOINT_TYPE_INTERRUPT_OUT 3
#define ENDPOINT_TYPE_BULK_IN 6
#define ENDPOINT_TYPE_BULK_OUT 2

#define INPUT_CONTEXT_A0A1_MASK 0b11

#define DESCRIPTOR_TYPE_DEVICE 1
#define DESCRIPTOR_TYPE_CONFIGURATION 2
#define DESCRIPTOR_TYPE_INTERFACE 4
#define DESCRIPTOR_TYPE_ENDPOINT 5
#define DESCRIPTOR_TYPE_SUPER_SPEED_ENDPOINT 0x30

#define CAPABILITY_ID_PROTOCOL 0x2
#define CAPABILITY_ID_USB_LEGACY_SUPPORT 0x1

typedef enum{
   PortSpeedLowSpeed = 2,
   PortSpeedFullSpeed = 1,
   PortSpeedHighSpeed = 3,
   PortSpeedSuperSpeed = 4,
}PortSpeed;


static int initBasePointers(const PciGeneralDeviceHeader *pciHeader, Xhci *xhci);
static void readPortInfo(Xhci *xhci);
static void waitForControllerReady(Xhci *xhci);
static void setMaxEnabledDeviceSlots(Xhci *xhci, int maxSlots);
static int getMaxEnabledDeviceSlots(Xhci *xhci);
static void resetXhc(Xhci *xhci);
static void initCommandRing(Xhci *xhci);
static void initEventRing(Xhci *xhci);
static void initDCAddressArray(Xhci *xhci);
static void turnOnController(Xhci *xhci);
static void initScratchPad(Xhci *xhci);
static int enablePort(Xhci *xhci, int portIndex);
static int isPortEnabled(Xhci *xhci, int portIndex);
static int checkoutPort(Xhci *xhci, int portIndex);

static int getSlotId(Xhci *xhci, uint8_t portNumber);
static int addressDevice(Xhci *xhci, int slotId, int portIndex);
static void initDefaultInputContext(XhcInputContext *inputContext, int portIndex, XhcdRing transferRing, PortSpeed speed);

static XhcStatus configureEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext);
static XhcStatus runConfigureEndpointCommand(Xhci *xhci, int slotId, XhcInputContext *inputContext);

static XhcStatus initInterruptEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext);
static XhcStatus initBulkEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext);

static int getEndpointIndex(UsbEndpointDescriptor *endpoint);

static XhcStatus initDevice(Xhci *xhci, int portIndex, XhcDevice *result);
static int getNewlyAttachedDevices(Xhci *xhci, uint32_t *result, int bufferSize);
static int setMaxPacketSize(Xhci *xhci, int slotId);

static void ringCommandDoorbell(Xhci *xhci);

static void test(Xhci *xhci);
static int putConfigTD(Xhci *xhci, int slotId, TD td);
static void xhcd_ringDoorbell(Xhci *xhci, uint8_t slotId, uint8_t target);
static XhcOutputContext *getOutputContext(Xhci *xhci, int slotId);

static PortStatusAndControll *getPortStatus(Xhci *xhci, int portNumber);
static PortSpeed getPortSpeed(Xhci *xhc, int portIndex);
static PortUsbType getUsbType(Xhci *xhci, int portNumber);
static PortUsbType getProtocolSlotType(Xhci *xhci, int portNumber);
//static int shouldEnablePort(Xhci *xhci, int portNumber);
//
static int port = 0;

__attribute__((aligned(64)))
static XhcInputContext inputContext[MAX_DEVICE_SLOTS_ENABLED];

static int count = 0;

static void handler(void *data){
//    XhcEventTRB result;
//    if(xhcd_readEvent(&xhci->eventRing, &result, 1)){
//       uintptr_t trbPtr = result.trbPointerLow | result.trbPointerHigh << 32;
//       TRB trb = *((TRB*)trbPtr);
//       printf("trb: %d\n", trb.type);
//    }
   printf("count %d\n", count++);
}

static int first;
XhcStatus xhcd_init(const PciDescriptor descriptor, Xhci *xhci){
   int errorCode = 0;
   PciGeneralDeviceHeader pciHeader;
   pci_getGeneralDevice(descriptor, &pciHeader);
   if((errorCode = initBasePointers(&pciHeader, xhci)) != 0){
      return errorCode;
   }
   first = 0; //FIXME: remove

   MsiXVectorData vectorData = pci_getDefaultMsiXVectorData(handler, xhci);
   MsiXDescriptor msiDescriptor;
   pci_initMsiX(&descriptor, &msiDescriptor);
   pci_setMsiXVector(msiDescriptor, 0, 33, vectorData);
   pci_enableMsiX(descriptor, msiDescriptor);

   waitForControllerReady(xhci);
   resetXhc(xhci);
   waitForControllerReady(xhci);
   setMaxEnabledDeviceSlots(xhci, getMaxEnabledDeviceSlots(xhci));
   initDCAddressArray(xhci);
   initScratchPad(xhci);


   readPortInfo(xhci);
   initCommandRing(xhci);
//    printf("init eventring status %X\n", xhci->operation->USBStatus);
   initEventRing(xhci);


//    for(int i = 1; i < 1024; i++){
//       InterrupterRegisters *interrupter = &xhci->interrupterRegisters[i];
//       interrupter->eventRingSegmentTableSize = 0;
//       interrupter->interruptEnable = 0;
//    }
//
   xhci->operation->USBCommand |= (1 << 2);
   turnOnController(xhci);
   

   xhci->operation->USBStatus |= 1 << 3;
   while(xhci->operation->USBStatus & (1<<3));
//    printf("xhc running %b\n", xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT);
//    printf("xhc halted %b\n", xhci->operation->USBStatus & 1);
//    printf("Error? %d\n", xhci->operation->USBStatus & (1 << 12));


//    for(int i = 0; i < 100; i++){
//       printf("-");
//       printf("\b");
//    }
//    printf("Conf %X\n", xhci->operation->configure);
//    printf("stat %X\n", xhci->operation->USBStatus);
//    printf("cmd %X\n", xhci->operation->USBCommand);

   uint32_t low = xhci->operation->commandRingControll;
   printf("cmdr: %X\n", low);

//    printf("op: %X\n", xhci->operation);
//    printf("xhci: %X\n", xhci);

//    uint32_t *ptr = (uint32_t*)&xhci->interrupterRegisters[0];
//    printf("intr: ");
//    for(int i = 0; i < 8; i++){
//       printf("%X ", *ptr);
//       ptr++;
//    }
//    printf("\n");

   printf("Testing command ring\n");
   void *curr = 0;
   printf("curr: %X\n", &curr);
   printf("func: %X\n", xhcd_init);

//    printf("op: %X\n", xhci->operation);
//    printf("xhci: %X\n", xhci);

//    for(int i = 0; i < 100; i++){
//       printf("%d",i);
//    }
//    printf("a");
//    printf("b");
//    printf("c");
//    printf("d");
//    printf("e");
//    printf("f");
//    printf("g");
//    printf("h");
//    printf("i");
//    printf("j");
//    printf("k");
//    printf("l");
//    printf("m");
//    printf("n");
//    printf("o");
//    printf("p");

   low = xhci->operation->commandRingControll;
   printf("cmdr: %X\n", low);
   uint32_t status = xhci->operation->USBStatus;
   printf("yes\n");
   printf("status: %X\n", status);
//    XhcEventTRB event;
//    xhcd_putTRB(TRB_NOOP(), &xhci->commandRing);
//    ringCommandDoorbell(xhci);;
//    while(1){
//       if(xhci->operation->USBStatus){
//          printf("status: %X\n", xhci->operation->USBStatus);
//          xhci->operation->USBStatus |= 1 << 3;
//          while(xhci->operation->USBStatus & (1<<3));
//       }
//       if(xhcd_readEvent(&xhci->eventRing, &event, 1)){
//          printf("event: %d, status %d\n", event.trbType, event.completionCode);
//       }
//    }

   
   //FIXME: a bit of hack, clearing event ring
   XhcEventTRB result[16];
//    while(!xhcd_readEvent(&xhci->eventRing, &result, 1) || result.trbType != 33);
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
   int count = xhci->enabledPorts;
//    while(1);
   uint8_t resultIndex = 0;
   for(uint8_t i = 0; i < count && resultIndex < bufferSize; i++){
      if(checkoutPort(xhci, i)){
         port = i;
         result[resultIndex] = i;
         resultIndex++;
      }
      
   }
   return resultIndex;
}

static void readPortInfo(Xhci *xhci){
   uint8_t maxPorts = xhci->enabledPorts;
   xhci->portInfo = calloc((maxPorts + 1) * sizeof(UsbPortInfo));
   uint32_t xECP = (xhci->capabilities->capabilityParams1.extendedCapabilitiesPointer << 2);
   uintptr_t address = (uintptr_t)xhci->capabilities;

   while(xECP){
      address += xECP;
      XhciExtendedCapabilities *cap = (XhciExtendedCapabilities *)(address);

      if(cap->capabilityId == CAPABILITY_ID_PROTOCOL){
         XhciXCapSupportedProtocol *sp = (XhciXCapSupportedProtocol*)cap;

         if(sp->compatiblePortOffset + sp->compatiblePortCount - 1 > maxPorts){
            printf("too many ports %d, expected %d\n",
                  sp->compatiblePortOffset + sp->compatiblePortCount,
                  maxPorts + 1);
         }
         else if(sp->revisionMajor != 0x3 && sp->revisionMajor != 0x2){
            printf("Unknown protocol %X\n", sp->revisionMajor);
         }
         else{
            for(int i = sp->compatiblePortOffset;
               i < sp->compatiblePortOffset + sp->compatiblePortCount;
               i++){
               xhci->portInfo[i].usbType = sp->revisionMajor;
               xhci->portInfo[i].protocolSlotType = sp->protocolSlotType;
            }
         }

      }
      xECP = cap->nextExtendedCapabilityPointer << 2;
   }
}
static XhcStatus initDevice(Xhci *xhci, int portIndex, XhcDevice *result){
   if(!enablePort(xhci, portIndex)){
      return XhcEnablePortError;
   }

   int slotId = getSlotId(xhci, portIndex + 1);
   if(slotId < 0){
      return XhcSlotIdError;
   }
   printf("delay\n");
   for(int i = 0; i < 10000; i++){
      printf("-\b");
   }

   if(!addressDevice(xhci, slotId, portIndex)){
      return XhcAddressDeviceError;
   }

//    FIXME:??
//    if(!setMaxPacketSize(xhci, slotId)){
//       return XhcSetMaxPacketSizeError;
//    }
   *result = (XhcDevice){
      .slotId = slotId,
      .portIndex = portIndex,
      .xhci = xhci,
      .portSpeed = getPortSpeed(xhci, portIndex),
   };
   return XhcOk;
}
#define REQUEST_SET_CONFIGURATION 9
TD TD_SET_CONFIGURATION(int configuration){
   SetupStageHeader header;
   header.bmRequestType = 0;
   header.bRequest = REQUEST_SET_CONFIGURATION;
   header.wValue = configuration;
   header.wIndex = 0;
   header.wLength = 0;
   TRB setupTrb = TRB_SETUP_STAGE(header);
   TRB statusTrb = TRB_STATUS_STAGE(1); //Direction in
   TD result = {{setupTrb, statusTrb}, 2};
   return result;
}
XhcStatus xhcd_setConfiguration(XhcDevice *device, const UsbConfiguration *configuration){
   XhcInputContext inputContext __attribute__((aligned(16)));
   memset((void*)&inputContext, 0, sizeof(XhcInputContext));
   printf("set config\n");
   for(int i = 0; i < configuration->descriptor.bNumInterfaces; i++){
      UsbInterface *interface = &configuration->interfaces[i];
      UsbInterfaceDescriptor desc = interface->descriptor;
      printf("interface %X %X %X\n", desc.bInterfaceClass, desc.bInterfaceSubClass, desc.bInterfaceProtocol);
      printf("n: %X, setting: %X\n", desc.bInterfaceNumber, desc.bAlternateSetting);
      for(int j = 0; j < interface->descriptor.bNumEndpoints; j++){
         UsbEndpointDescriptor *endpointDescriptor = &interface->endpoints[j];
         printf("config %X\n", endpointDescriptor->bmAttributes);
         int status = configureEndpoint(device->xhci, device->slotId, endpointDescriptor, &inputContext);
         if(status != XhcOk){
            return status;
         }
      }
   }
   XhcStatus status = runConfigureEndpointCommand(device->xhci, device->slotId, &inputContext);
   if(status != XhcOk){
      printf("Endpoint config error!");
      return status;
   }
   TD td = TD_SET_CONFIGURATION(configuration->descriptor.bConfigurationValue);
   if(!putConfigTD(device->xhci, device->slotId, td)){
      printf("[xhc] failed to set configuration\n");
      return XhcNotYetImplemented; //FIXME: Wrong error code

   }
   return XhcOk;
}
static XhcStatus configureEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext){
   switch(endpoint->transferType){
      case ENDPOINT_TRANSFER_TYPE_INTERRUPT:
         return initInterruptEndpoint(xhci, slotId, endpoint, inputContext);
      case ENDPOINT_TRANSFER_TYPE_BULK:
         return initBulkEndpoint(xhci, slotId, endpoint, inputContext);
      default:
         printf("Transfer type not yet implemented %d\n", endpoint->transferType);
         return XhcNotYetImplemented;
   }
}
XhcStatus xhcd_readData(const XhcDevice *device, UsbEndpointDescriptor endpoint, void *dataBuffer, uint16_t bufferSize){
   int endpointIndex = getEndpointIndex(&endpoint);

   Xhci *xhci = device->xhci;

   TRB trb = TRB_NORMAL(dataBuffer, bufferSize);
   XhcdRing *transferRing = &xhci->transferRing[device->slotId][endpointIndex - 1];
   xhcd_putTRB(trb, transferRing);
   xhcd_ringDoorbell(xhci, device->slotId, endpointIndex);

   XhcEventTRB event;
   while(!xhcd_readEvent(&xhci->eventRing, &event, 1));

   return XhcOk;
}
XhcStatus xhcd_writeData(const XhcDevice *device,
      UsbEndpointDescriptor endpoint,
      void *dataBuffer,
      uint16_t bufferSize){

   int endpointIndex = getEndpointIndex(&endpoint);
   Xhci *xhci = device->xhci;
   TRB trb = TRB_NORMAL(dataBuffer, bufferSize);
   XhcdRing *transferRing = &xhci->transferRing[device->slotId][endpointIndex - 1];
   xhcd_putTRB(trb, transferRing);
   xhcd_ringDoorbell(xhci, device->slotId, endpointIndex);

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
   uint8_t statusDirection = 1; //In
   uint8_t dataDirection = 0; //Out
   if((request.bmRequestType & (1<<7))){ //Device-to-host
      if(request.wLength > 0){
         statusDirection = 0; //Out
      }
      dataDirection = 1; //In
   }
   TRB statusTrb = TRB_STATUS_STAGE(statusDirection);

   TD td;
   if(header.wLength == 0){
      td = (TD){{setupTrb, statusTrb}, 2};
   }else{
      TRB dataTrb = TRB_DATA_STAGE((uintptr_t)request.dataBuffer, request.wLength, dataDirection);
      td = (TD){{setupTrb, dataTrb, statusTrb}, 3};
   }
   if(!putConfigTD(device->xhci, device->slotId, td)){
      return XhcSendRequestError;

   }
   return XhcOk;
}
/*
 * [  ]64-bit
 * [X] superspeed stuff p.179
 * [ ] Not enough power? look in configuration descriptor
 * [X] Get interface? Am i using the correct one?
 * [ ] p.101 SetAddress timing?
 * [ ] p.192 Direction of setup stage
 */
void xhc_dumpCapabilityRegs(Xhci *xhci){
   XhciCapabilities *caps = xhci->capabilities;
   printf("capability params (HCCPARAMS): ");
   printf("1: %X. 2: %X\n", caps->capabilityParams1, caps->capabilityParams2);
   printf("struct params (HCSPARAMS): ");
   printf("1: %X. 2: %X. 3: %X\n", caps->structParams1, caps->structParams2, caps->structParams3);
}
void xhc_dumpOperationalRegs(Xhci *xhci){
   XhciOperation *op = xhci->operation;
   printf("USBCMD: %X\n", op->USBCommand);
   printf("USBSTS: %X\n", op->USBStatus);
   printf("pageSize: %X\n", op->pageSize);
   printf("DNCTRL: %X\n", op->deviceNotificationControll);
   printf("CRCR: %X %X\n", op->commandRingControll);
   printf("DCBAAP: %X %X\n", op->dcAddressArrayPointer);
   printf("CONFIG: %X\n", op->configure);
}
static XhcStatus initInterruptEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext){
   printf("inendpoint. In? : %b\n", endpoint->direction == ENDPOINT_DIRECTION_IN);
   printf("addr: %X\n", endpoint->bEndpointAddress);
   int endpointIndex = getEndpointIndex(endpoint);

   XhcdRing transferRing = xhcd_newRing(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhci->transferRing[slotId][endpointIndex - 1] = transferRing;

   uint32_t maxPacketSize = endpoint->wMaxPacketSize & 0x7FF;
   uint32_t maxBurstSize = (endpoint->wMaxPacketSize & 0x1800) >> 11;
   uint32_t maxESITPayload = maxPacketSize * (maxBurstSize + 1);
   uint32_t endpointType =
      endpoint->direction == ENDPOINT_DIRECTION_IN ? ENDPOINT_TYPE_INTERRUPT_IN : ENDPOINT_TYPE_INTERRUPT_OUT;

   printf("enpointType %d\n (7)", endpointType);
   printf("max packet size: %X,", maxPacketSize);
   printf("max burst size: %X,", maxBurstSize);
   printf("maxESITPayload: %X,", maxESITPayload);
   printf("interval: %X\n", endpoint->bInterval);
   XhcEndpointContext *endpointContext = &inputContext->endpointContext[endpointIndex - 1];
   *endpointContext = (XhcEndpointContext){
      .endpointType = endpointType,
      .maxPacketSize = maxPacketSize,
      .maxBurstSize = maxBurstSize,
      .errorCount = 3,
      .dequeuePointer = (uintptr_t)transferRing.dequeue | transferRing.pcs, //X
      .maxESITPayloadLow = (uint16_t)maxESITPayload,
      .maxESITPayloadHigh = (uint16_t)(maxESITPayload >> 16),
      .interval = 6, //FIXME
      .avarageTrbLength = maxESITPayload,
   };
   inputContext->inputControlContext.addContextFlags |= 1 << endpointIndex;
   return XhcOk;
}
static XhcStatus initBulkEndpoint(Xhci *xhci, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext){
   int endpointIndex = getEndpointIndex(endpoint);

   inputContext->inputControlContext.addContextFlags |= 1 << endpointIndex;

   uint32_t maxBurstSize = 0;
   uint32_t maxPrimaryStreams = 0;
   uintptr_t dequePointer = 0;
   uint32_t hostInitiateDisable = 0;
   uint32_t linearStreamArray = 0;

   if(endpoint->superSpeedDescriptor){
      maxBurstSize = endpoint->superSpeedDescriptor->bMaxBurst;

      if(endpoint->superSpeedDescriptor->maxStreams > 0){
         printf("Streams not yet implemented\n");
         return XhcNotYetImplemented;

      }
      else{
         XhcdRing transferRing = xhcd_newRing(DEFAULT_TRANSFER_RING_TRB_COUNT);
         xhci->transferRing[slotId][endpointIndex - 1] = transferRing;

         maxPrimaryStreams = 0;
         dequePointer = (uintptr_t)transferRing.dequeue | transferRing.pcs;
     }
   }
   else{
      maxBurstSize = 0;
   }

   XhcEndpointContext *endpointContext = &inputContext->endpointContext[endpointIndex - 1];
   *endpointContext = (XhcEndpointContext)
   {
      .endpointType = endpoint->direction == ENDPOINT_DIRECTION_IN ? ENDPOINT_TYPE_BULK_IN : ENDPOINT_TYPE_BULK_OUT,
      .maxPacketSize = endpoint->wMaxPacketSize,
      .maxBurstSize = maxBurstSize,
      .errorCount = 3,
      .maxPrimaryStreams = maxPrimaryStreams,
      .dequeuePointer = dequePointer,
      .hostInitiateDisable = hostInitiateDisable,
      .linearStreamArray = linearStreamArray,
   };
   return XhcOk;
}

static int getSlotId(Xhci *xhci, uint8_t portNumber){
   printf("Getting slot id\n");
   xhcd_putTRB(TRB_ENABLE_SLOT(getProtocolSlotType(xhci, portNumber)), &xhci->commandRing);
   ringCommandDoorbell(xhci);
   XhcEventTRB trb;
   //FIXME: hack
   printf("waiting!");
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
   printf("Slot id %d\n", trb.slotId);
   return trb.slotId;
}
static PortSpeed getPortSpeed(Xhci *xhc, int portIndex){
   PortStatusAndControll portStatus = xhc->operation->ports[portIndex].statusAndControll;
   PortSpeed speed = portStatus.portSpeed;
   printf("speed: %X\n", speed);
   return speed;
}
static PortUsbType getUsbType(Xhci *xhci, int portNumber){
   return xhci->portInfo[portNumber].usbType;
}
static PortUsbType getProtocolSlotType(Xhci *xhci, int portNumber){
   return xhci->portInfo[portNumber].protocolSlotType;
}
static void initDefaultInputContext(XhcInputContext *inputContext, int portIndex, XhcdRing transferRing, PortSpeed speed){
   memset((void*)inputContext, 0, sizeof(XhcInputContext));
   inputContext->inputControlContext.addContextFlags |= INPUT_CONTEXT_A0A1_MASK;

   XhcSlotContext *slotContext = &inputContext->slotContext;
   slotContext->routeString = 0;
   slotContext->speed = speed;
   slotContext->contextEntries = 1;
   slotContext->rootHubPortNumber = portIndex + 1; //port number is 1 indexed

   //FIXME: Is just 8 valid?
   uint16_t maxPacketSize;
   switch(speed){
      case PortSpeedLowSpeed:
         maxPacketSize = 8;
         break;
      case PortSpeedFullSpeed:
      case PortSpeedHighSpeed:
         maxPacketSize = 64;
         break;
      case PortSpeedSuperSpeed:
         maxPacketSize = 512;
         break;
      default:
         printf("Unknown speed %X\n", speed);
         while(1);
   }

   XhcEndpointContext *controlEndpoint = &inputContext->endpointContext[0];
   controlEndpoint->endpointType = ENDPOINT_TYPE_CONTROL;
   controlEndpoint->maxPacketSize = maxPacketSize;
   controlEndpoint->dequeuePointer = (uintptr_t)transferRing.dequeue | transferRing.pcs;
   controlEndpoint->errorCount = 3;
   controlEndpoint->avarageTrbLength = 8;
}
static int addressDevice(Xhci *xhci, int slotId, int portIndex){
   XhcInputContext inputContext __attribute__((aligned(16)));
   memset((void*)&inputContext, 0, sizeof(XhcInputContext));

   printf("Address device\n");
   XhcOutputContext *outputContext = callocco(sizeof(XhcOutputContext), 64, 0);
   xhci->dcBaseAddressArray[slotId] = (uintptr_t)outputContext;

   XhcdRing transferRing = xhcd_newRing(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhci->transferRing[slotId][0] = transferRing;
   printf("New ring\n");

   PortSpeed speed = getPortSpeed(xhci, portIndex);
   initDefaultInputContext(&inputContext, portIndex, transferRing, speed);
   xhcd_putTRB(TRB_ADDRESS_DEVICE((uintptr_t)&inputContext, slotId, 0), &xhci->commandRing);
   ringCommandDoorbell(xhci);

   printf("init context (waiting)\n");
   XhcEventTRB result;
   while(xhcd_readEvent(&xhci->eventRing, &result, 1) == 0);
   if(result.completionCode != Success){
      printf("[xhc] failed to addres device (Event: %X %X %X %X, code: %d)\n", result, result.completionCode);
      return 0;
   }
//    printf("init (=#(?/#\n");

//    uint8_t buffer[8];
//    xhcd_putTD(TD_GET_DESCRIPTOR(buffer, sizeof(buffer)), &transferRing);
//    xhcd_ringDoorbell(xhci, slotId, 1);

//    while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
//    if(result.completionCode != Success){
//       printf("[xhc] failed to get max packet size\n");
//       return 0;
//    }
//    uint8_t maxPacketSize = buffer[7];
//    inputContext.endpointContext[0].maxPacketSize = maxPacketSize;


//    xhcd_putTRB(TRB_ADDRESS_DEVICE((uintptr_t)&inputContext, slotId, 0), &xhci->commandRing);
//    ringCommandDoorbell(xhci);

//    while(xhcd_readEvent(&xhci->eventRing, &result, 1) == 0);
//    if(result.completionCode != Success){
//       printf("[xhc] failed to addres device (Event: %X %X %X %X, code: %d)\n", result, result.completionCode);
//       return 0;
//    }
   printf("[xhc] successfully addressed device: (Event: %X %X %X %X)\n", result);
   return 1;
}
static int checkoutPort(Xhci *xhci, int portIndex){
   XhciPortRegisters *port = &xhci->operation->ports[portIndex];
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
   while((xhci->operation->USBCommand & (1<<1)));
}
static void initCommandRing(Xhci *xhci){
   xhci->commandRing = xhcd_newRing(DEFAULT_COMMAND_RING_SIZE);
   xhcd_attachCommandRing(xhci->operation, &xhci->commandRing);
}
static void initEventRing(Xhci *xhci){
   xhci->eventRing = xhcd_newEventRing(DEFAULT_EVENT_SEGEMNT_TRB_COUNT);
   uint32_t *ptr = xhci->interrupterRegisters;
   *ptr |= 2;
   xhcd_attachEventRing(&xhci->eventRing, &xhci->interrupterRegisters[0]);
   uint32_t rings = xhci->capabilities->structParams1.maxInterrupters;
   printf("eventRings: %X\n", rings);
}
static int enablePort(Xhci *xhci, int portIndex){
   if(isPortEnabled(xhci, portIndex)){
      printf("Port already enabled\n");
      return 1;
   }
   PortUsbType usbType = getUsbType(xhci, portIndex + 1);
   if(usbType == PortUsbType3){
      printf("(USB3) port should already be enabled, index: %X\n", portIndex);
      return 0;
   }
/*   if(!shouldEnablePort(xhci, portNumber)){ //FIXME: This should worrk
      printf("Failed to enable port (USB3)\n");
      return 0;
   }*/
   printf("Enabling port (USB2) %X\n", portIndex);
   PortStatusAndControll *statusAndControll = getPortStatus(xhci, portIndex);
   PortStatusAndControll temp = *statusAndControll;

   temp.portEnabledDisabled = 0;
   temp.connectStatusChange = 0;
   temp.portEnableDisableChange = 0;
   temp.warmPortResetChange = 0;
   temp.overCurrentChange = 0;
   temp.portResetChange = 0;
   temp.portLinkStateChange = 0;
   temp.portConfigErrorChange = 0;

   temp.portReset = 1;

   *statusAndControll = temp;
   while(!temp.portResetChange){
      temp = *statusAndControll;
   }
   while(!temp.portEnabledDisabled){
      temp = *statusAndControll;
   }
   int isEnabled = isPortEnabled(xhci, portIndex);

   temp.portEnabledDisabled = 0;
   temp.portReset = 0;
   temp.connectStatusChange = 0;
   temp.portEnableDisableChange = 0;
   temp.warmPortResetChange = 0;
   temp.overCurrentChange = 0;
   temp.portLinkStateChange = 0;
   temp.portConfigErrorChange = 0;

   temp.portResetChange = 1; //Clear
   *statusAndControll = temp;
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
static XhcStatus runConfigureEndpointCommand(Xhci *xhci, int slotId, XhcInputContext *inputContext){
   XhcOutputContext *output = getOutputContext(xhci, slotId);
   uint32_t contextEntries = output->slotContext.contextEntries;
   uint32_t maxAddContextEntries = 0;
   for(int i = 0; i < 32; i++){
      if(inputContext->inputControlContext.addContextFlags & (1 << i)){
         maxAddContextEntries = i;
      }
   }
   if(maxAddContextEntries > contextEntries){
      inputContext->inputControlContext.addContextFlags |= 1;
      inputContext->slotContext = output->slotContext;
      inputContext->slotContext.contextEntries = maxAddContextEntries; //FIXME: Double check +1 (p.84)?
   }

//    inputContext->inputControlContext.configurationValue = 1; //FIXME: use correct value

   TRB trb = TRB_CONFIGURE_ENDPOINT((void*)inputContext, slotId);
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
   XhcEventTRB event;
   while(xhcd_readEvent(&xhci->eventRing, &event, 1)); //FIXME: Hack

   XhcdRing *transferRing = &xhci->transferRing[slotId][0];
   xhcd_putTD(td, transferRing);
   xhcd_ringDoorbell(xhci, slotId, 1);

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
   if(pciHeader->baseAddress[1] != 0){
      printf("Error: unable to reach xhcd MMIO in 32 bit mode: %X %X\n",
            pciHeader->baseAddress[0], pciHeader->baseAddress[1]);
      return -1;
   }

   uint32_t base = pciHeader->baseAddress[0] & (~0xFF); 

   
   xhci->capabilities = (XhciCapabilities *)base;
   uint32_t capLength = xhci->capabilities->capabilityRegistersLength;
   uint32_t doorbellOffset = xhci->capabilities->doorbellOffset & ~0x3;
   uint32_t runtimeRegOffset = xhci->capabilities->runtimeRegisterSpaceOffset  & ~0x1F;

   xhci->operation = (XhciOperation *) (base + capLength);
   xhci->doorbells = (XhciDoorbell *) (base + doorbellOffset);
   xhci->interrupterRegisters = (InterrupterRegisters *)(base + runtimeRegOffset + 0x20);

   printf("cap1: %X\n", xhci->capabilities->capabilityParams1);
   uint32_t xECP = (xhci->capabilities->capabilityParams1.extendedCapabilitiesPointer << 2);
   printf("xECP: %X\n", xECP);
   volatile uint32_t *cap = (uint32_t *)(base + xECP);
   if(xECP != 0){
      uint32_t i = 0;
      while(i < 5){
         i++;
         printf("cap: %X\n", *cap);
         if((*cap & 0xFF) == 1){
            printf("%X\n", *(cap));
            printf("%X\n", *(cap + 1));

            *cap |= 1 << 24;
            while((*cap & (1 << 24)) == 0);
            while((*cap & (1 << 16)));

            *(cap + 1) = 0;

            printf("%X\n", *(cap));
            printf("%X\n", *(cap + 1));
         }


         uint8_t next = ((*cap >> 8) & 0xFF);
         printf("next: %X\n", next);
         if(next == 0){
            break;
         }
         cap += next;
      }

   }

   return 0;
}
static void waitForControllerReady(Xhci *xhci){
   while(xhci->operation->USBStatus & CNR_FLAG);
}
static int getMaxEnabledDeviceSlots(Xhci *xhci){
   StructParams1 structParams1 = xhci->capabilities->structParams1;
   return structParams1.maxPorts;
}
static void setMaxEnabledDeviceSlots(Xhci *xhci, int maxSlots){
   assert((xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT) == 0);

   xhci->enabledPorts = maxSlots;
   volatile uint32_t *configure = (uint32_t*)&xhci->operation->configure;
   uint32_t val = *configure;
   val &=  ~0xFF;
   val |= maxSlots;
   *configure = val;
   printf("MaxSlotsEn: %X\n", maxSlots);
}
static uint32_t getPageSize(Xhci *xhci){
   return xhci->operation->pageSize << 12;
}
static void initDCAddressArray(Xhci *xhci){
   assert((xhci->operation->USBCommand & USBCMD_RUN_STOP_BIT) == 0);

   uint8_t maxSlots = xhci->operation->configure.enabledDeviceSlots;
   uint32_t pageSize = getPageSize(xhci);
   uint32_t arraySize = (maxSlots + 1) * 64;
   xhci->dcBaseAddressArray = callocco(arraySize, 64, pageSize);
   xhci->operation->dcAddressArrayPointer = (uintptr_t)xhci->dcBaseAddressArray; 
}

static void initScratchPad(Xhci *xhci){
   StructParams2 structParams2 = xhci->capabilities->structParams2;
   uint32_t scratchpadSize = structParams2.maxScratchpadBuffersHigh << 5;
   scratchpadSize |= structParams2.maxScratchpadBuffersLow;
   printf("Required scratchpadSize %d\n", scratchpadSize);

   uint32_t pageSize = getPageSize(xhci);
   printf("PageSize: %d %d\n", pageSize, xhci->operation->pageSize);
   volatile uint64_t *scratchpadPointers = mallocco(scratchpadSize * sizeof(uint64_t), 64, pageSize);

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
