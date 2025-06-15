#include "kernel/xhcd.h"
#include "kernel/xhcd-ring.h"
#include "kernel/xhcd-event-ring.h"
#include "kernel/usb-descriptors.h"
#include "string.h"
#include "kernel/interrupt.h"
#include "kernel/paging.h"
#include "kernel/kernel-io.h"
#include "kernel/logging.h"
#include "kernel/memory.h"
#include "stdlib.h"
#include "utils/utils.h"


//FIXME: remove
#include "kernel/pci.h"

#include "utils/assert.h"
//DC = Device context
//p.168. TRB rings shall not cross 64KB boundary
//p82 UBS LS FS HS requres port process to advance to enabled state


#define CNR_FLAG (1<<11)

#define MAX_DEVICE_SLOTS_ENABLED 16
#define DEFAULT_COMMAND_RING_SIZE 32
#define DEFAULT_EVENT_RING_TRB_COUNT 128
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

#define RETRY_WITH_DELAY(condition, count, delay) ({ \
    bool success = false; \
    for (int i = 0; i < (count); i++) { \
        if (condition) { success = true; break; } \
        thread_sleep(delay); \
    } \
    success; \
})

typedef enum{
   PortSpeedLowSpeed = 2,
   PortSpeedFullSpeed = 1,
   PortSpeedHighSpeed = 3,
   PortSpeedSuperSpeed = 4,
}PortSpeed;


static int doBiosHandoff(Xhcd *xhcd);
static void readPortInfo(Xhcd *xhcd);
static XhcStatus waitForControllerReady(Xhcd *xhcd);
static XhcStatus setMaxEnabledDeviceSlots(Xhcd *xhcd, uint8_t maxSlots);
static int getMaxEnabledDeviceSlots(Xhcd *xhcd);
static XhcStatus resetXhc(Xhcd *xhcd);
static XhcStatus initCommandRing(Xhcd *xhcd);
static XhcStatus initEventRing(Xhcd *xhcd);
static XhcStatus initDCAddressArray(Xhcd *xhcd);
static void turnOnController(Xhcd *xhcd);
static XhcStatus initScratchPad(Xhcd *xhcd);
static uint32_t getScratchpadSize(Xhcd *xhcd);
static int enablePort(Xhcd *xhcd, int portIndex);
static int isPortEnabled(Xhcd *xhcd, int portIndex);
static int checkoutPort(Xhcd *xhcd, int portIndex);

static int getSlotId(Xhcd *xhcd, uint8_t portNumber);
static int addressDevice(Xhcd *xhcd, int slotId, int portIndex);
static void initDefaultInputContext(XhcInputContext *inputContext, int portIndex, XhcdRing *transferRing, PortSpeed speed);

static XhcStatus configureEndpoint(Xhcd *xhcd, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext);
static XhcStatus runConfigureEndpointCommand(Xhcd *xhcd, int slotId, XhcInputContext *inputContext);

static XhcStatus initInterruptEndpoint(Xhcd *xhcd, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext);
static XhcStatus initBulkEndpoint(Xhcd *xhcd, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext);

static int getEndpointIndex(UsbEndpointDescriptor *endpoint);

static XhcStatus initDevice(Xhcd *xhcd, int portIndex, XhcDevice *result);
static int getNewlyAttachedDevices(Xhcd *xhcd, uint32_t *result, int bufferSize);
// static int setMaxPacketSize(Xhcd *xhcd, int slotId);

static void ringCommandDoorbell(Xhcd *xhcd);

static int putConfigTD(Xhcd *xhcd, int slotId, TD td);
static void xhcd_ringDoorbell(Xhcd *xhcd, uint8_t slotId, uint8_t target);
static XhcOutputContext *getOutputContext(Xhcd *xhcd, int slotId);

// static PortStatusAndControll *getPortStatus(Xhcd *xhcd, int portNumber);
static PortSpeed getPortSpeed(Xhcd *xhc, int portIndex);
static PortUsbType getUsbType(Xhcd *xhcd, int portNumber);
static PortUsbType getProtocolSlotType(Xhcd *xhcd, int portNumber);
//static int shouldEnablePort(Xhci *xhcd, int portNumber);

static bool assert_physical_continguous(uintptr_t start, size_t length);

static int port = 0;

__attribute__((aligned(64))) static XhcInputContext inputContext[MAX_DEVICE_SLOTS_ENABLED];

static int count = 0;

XhcStatus xhcd_setInterrupter(XhcDevice *device, int endpoint, void (*handler)(void *), void *data){
   XhcInterruptHandler interruptHandler = {
      .handler = handler,
      .data = data
   };
   Xhcd *xhcd = device->data;
   xhcd->handlers[device->slotId * 32 + endpoint] = interruptHandler;
   return XhcOk;
}

static int dequeEventTrb(Xhcd *xhcd, XhcEventTRB *result){
   semaphore_aquire(xhcd->eventSemaphore);

   uint32_t advancedDequeue = (xhcd->eventBufferDequeueIndex + 1) % xhcd->eventBufferSize;
   if(advancedDequeue == xhcd->eventBufferEnqueueIndex){
      return 0;
   }
   *result = xhcd->eventBuffer[advancedDequeue];
   xhcd->eventBufferDequeueIndex = advancedDequeue;
   return 1;
}

static void handler(void *data){
   Xhcd *xhcd = (Xhcd*)data;
   do{
      XhcEventTRB events[32];
      int count = xhcdEventRing_read(xhcd->eventRing, events, 32);
      for(int i = 0; i < count; i++){
         uint32_t endpoint = events[i].endpointId;
         uint32_t slotId = events[i].slotId;

         XhcInterruptHandler handler = xhcd->handlers[slotId * 32 + endpoint];
         if(handler.handler && handler.data){
            handler.handler(handler.data);
         }
         //This whole buffer thing is a temporary solution
         assert(xhcd->eventBufferDequeueIndex == xhcd->eventBufferDequeueIndex); 

         loggDebug("event from %X", events[i].trbPointerLow);
         xhcd->eventBuffer[xhcd->eventBufferEnqueueIndex] = events[i];
         xhcd->eventBufferEnqueueIndex = (xhcd->eventBufferEnqueueIndex + 1) % xhcd->eventBufferSize;
         semaphore_release(xhcd->eventSemaphore);
      }
   }while(count != 0);
}

XhcStatus xhcd_init(const PciDescriptor descriptor, Xhci *xhci){
   XhcStatus status = XhcOk;

   logging_startContext("xhcd_init")
   {
      loggDebug("Init");

      Xhcd *xhcd = kmalloc(sizeof(Xhcd));
      if(!xhcd){
         return XhcUnableToAllocateMemory;
      }
      memset(xhcd, 0, sizeof(Xhcd));
      xhci->data = xhcd;
      xhcd->eventBufferSize = 32;
      xhcd->eventBufferDequeueIndex = 0;
      xhcd->eventBufferEnqueueIndex = 1;
      xhcd->eventBuffer = kmalloc(sizeof(XhcEventTRB) * xhcd->eventBufferSize);
      if(!xhcd->eventBuffer){
         kfree(xhcd);
         return XhcUnableToAllocateMemory;
      }
      xhcd->eventSemaphore = semaphore_new(0);
      if(!xhcd->eventSemaphore){
         kfree(xhcd);
         kfree((void*)xhcd->eventBuffer);
         return XhcUnableToAllocateMemory;
      }


      PciGeneralDeviceHeader pciHeader;
      pci_getGeneralDevice(descriptor, &pciHeader);
      xhcd->hardware = xhcd_initRegisters(pciHeader);

      doBiosHandoff(xhcd); //TODO: verify

      //TODO: verify
      if(pci_isMsiXPresent(descriptor)){
         loggInfo("Using msix");
         MsiXVectorData vectorData = pci_getDefaultMsiXVectorData(handler, xhcd);
         MsiXDescriptor msiDescriptor;
         pci_initMsiX(&descriptor, &msiDescriptor);
         pci_setMsiXVector(msiDescriptor, 0, vectorData);
         pci_enableMsiX(descriptor, msiDescriptor);
      }else if(pci_isMsiPresent(descriptor)){
         loggInfo("Using msi");
         MsiInitData initData = pci_getDefaultSingleHandlerMsiInitData(handler, xhcd);
         MsiDescriptor result;
         pci_initMsi(descriptor, &result, initData);
      }else{
         loggError("Unable to init msi ans msix. This situaion is not implemented");
         while(1);
      }

      if((status = waitForControllerReady(xhcd)) != XhcOk
      || (status = resetXhc(xhcd)) != XhcOk
      || (status = waitForControllerReady(xhcd)) != XhcOk){
         goto cleanup_xhcd;
      }

      loggDebug("Controller ready");

      uint32_t devices = getMaxEnabledDeviceSlots(xhcd);
      if((status = setMaxEnabledDeviceSlots(xhcd, devices)) != XhcOk){
         goto cleanup_xhcd;
      }
      xhcd->handlers = kmalloc(devices * 32 * sizeof(XhcInterruptHandler));
      if(!xhcd->handlers){
         status = XhcUnableToAllocateMemory;
         goto cleanup_xhcd;
      }
      memset(xhcd->handlers, 0, devices * 32 * sizeof(XhcInterruptHandler));

      if((status = initDCAddressArray(xhcd)) != XhcOk){
         goto cleanup_xhcd;
      }
      status = initScratchPad(xhcd);
      if(status != XhcOk){
         goto cleanup_xhcd;
      }

      readPortInfo(xhcd); //Maybe? TODO: Check
      if((status = initCommandRing(xhcd)) != XhcOk){
         goto cleanup_xhcd;
      }
      if((status = initEventRing(xhcd)) != XhcOk){
         goto cleanup_xhcd;
      }

      // enable interrupts
      xhcd_orRegister(xhcd->hardware, USBCommand, (1 << 2));

      turnOnController(xhcd);

      //FIXME: Check
      xhcd_orRegister(xhcd->hardware, USBStatus, 1 << 3);
      while(xhcd_readRegister(xhcd->hardware, USBStatus) & (1<<3));

      //FIXME: a bit of hack, clearing event ring
      XhcEventTRB result[16];
      while(xhcdEventRing_read(xhcd->eventRing, result, 16));

      loggInfo("Controller turned on");
      lreturn XhcOk;

cleanup_xhcd:
      xhcd_free(xhci);
      kfree(xhcd);
   }

   return status;
}

void xhcd_free(Xhci *xhci){
   Xhcd *xhcd = xhci->data;
   if(xhcd->portInfo){
      kfree(xhcd->portInfo);
   }

   if(xhcd->dcBaseAddressArray){
      uint64_t *array = xhcd->dcBaseAddressArray;


      for(size_t i = 1; i < xhcd->dcBaseAddressArraySize; i++){
         if(array[i]){
            loggWarning("Free device context %X not implemented", array[i]);
         }
      }
      kfree(array);

   }

   if(xhcd->scratchpadArray){
      kfree(xhcd->scratchpadArray);
   }
   if(xhcd->scratchpadPointers){
      size_t scratchpadSize = getScratchpadSize(xhcd);
      for(size_t i = 0; i < scratchpadSize; i++){
         if(xhcd->scratchpadPointers[i]){
            kfree(xhcd->scratchpadPointers[i]);
         }
      }
      kfree(xhcd->scratchpadPointers);
   }

   XhcdRing **ringArray = (XhcdRing **)xhcd->transferRing;
   for(size_t i = 0; i < sizeof(xhcd->transferRing) / sizeof(XhcdRing *); i++){
      if(ringArray){
         xhcdRing_free(ringArray[i]);
      }
   }

   loggWarning("Unable to free event ring");

   if(xhcd->commandRing){
      xhcdRing_free(xhcd->commandRing);
   }

   if(xhcd->eventRing){
      xhcdEventRing_free(xhcd->eventRing);
   }

   if(xhcd->handlers){
      kfree(xhcd->handlers);
   }

   if(xhcd->eventBuffer){
      kfree((void*)xhcd->eventBuffer);
   }

   if(xhcd->eventSemaphore){
      semaphore_free(xhcd->eventSemaphore);
   }

   kfree(xhcd);

   loggInfo("Xhc free");
}

int xhcd_getDevices(Xhci *xhci, XhcDevice *resultBuffer, int bufferSize){
   Xhcd *xhcd = xhci->data;

   uint32_t *portIndexes = kmalloc(bufferSize * sizeof(uint32_t));
   int count = getNewlyAttachedDevices(xhcd, portIndexes, bufferSize);
   for(int i = 0; i < count; i++){
      XhcStatus status = initDevice(xhcd, portIndexes[i], &resultBuffer[i]);

      if(status != XhcOk){
         i--;
         count--;
         //FIXME: Some kind of error message here?
      }
   }
   kfree(portIndexes);
   return count;
}
static int getNewlyAttachedDevices(Xhcd *xhcd, uint32_t *result, int bufferSize){
   int count = xhcd->enabledPorts;
   //    while(1);
   uint8_t resultIndex = 0;
   for(uint8_t i = 0; i < count && resultIndex < bufferSize; i++){
      if(checkoutPort(xhcd, i)){
         port = i;
         result[resultIndex] = i;
         resultIndex++;
      }

   }
   return resultIndex;
}
static void readPortInfo(Xhcd *xhcd){
   uint8_t maxPorts = xhcd->enabledPorts;
   xhcd->portInfo = kcalloc((maxPorts + 1) * sizeof(UsbPortInfo));

   XhcExtendedCapabilityEnumerator enumerator = xhcd_newExtendedCapabilityEnumerator(xhcd->hardware);

   while(xhcd_hasNextExtendedCapability(&enumerator)){
      XhciExtendedCapabilities cap;
      xhcd_readExtendedCapability(&enumerator, (void*)&cap, sizeof(cap));

      if(cap.capabilityId == CAPABILITY_ID_PROTOCOL){
         XhciXCapSupportedProtocol sp;
         xhcd_readExtendedCapability(&enumerator, (void*)&sp, sizeof(sp));

         if(sp.compatiblePortOffset + sp.compatiblePortCount - 1 > maxPorts){
            loggWarning("too many ports %d, expected %d",
                  sp.compatiblePortOffset + sp.compatiblePortCount,
                  maxPorts + 1);
         }
         else if(sp.revisionMajor != 0x3 && sp.revisionMajor != 0x2){
            loggWarning("Unknown protocol %X", sp.revisionMajor);
         }
         else{
            for(int i = sp.compatiblePortOffset;
                  i < sp.compatiblePortOffset + sp.compatiblePortCount;
                  i++){
               xhcd->portInfo[i].usbType = sp.revisionMajor;
               xhcd->portInfo[i].protocolSlotType = sp.protocolSlotType;
            }
         }
         return;
      }
      xhcd_advanceExtendedCapabilityEnumerator(&enumerator);
   }
}
static XhcStatus initDevice(Xhcd *xhcd, int portIndex, XhcDevice *result){
   if(!enablePort(xhcd, portIndex)){
      return XhcEnablePortError;
   }

   int slotId = getSlotId(xhcd, portIndex + 1);
   if(slotId < 0){
      return XhcSlotIdError;
   }
   //FIXME: delay
   for(int i = 0; i < 10000; i++){
      kprintf("-\b");
   }

   if(!addressDevice(xhcd, slotId, portIndex)){
      return XhcAddressDeviceError;
   }

   //    FIXME:??
   //    if(!setMaxPacketSize(xhci, slotId)){
   //       return XhcSetMaxPacketSizeError;
   //    }
   *result = (XhcDevice){
      .slotId = slotId,
         .portIndex = portIndex,
         .data = xhcd,
         .portSpeed = getPortSpeed(xhcd, portIndex),
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
   Xhcd *xhcd = device->data;
   XhcInputContext inputContext __attribute__((aligned(16)));
   memset((void*)&inputContext, 0, sizeof(XhcInputContext));
   loggDebug("Set configuration");
   for(int i = 0; i < configuration->descriptor.bNumInterfaces; i++){
      UsbInterface *interface = &configuration->interfaces[i];
      UsbInterfaceDescriptor desc = interface->descriptor;
      loggDebug("Interface %X %X %X", desc.bInterfaceClass, desc.bInterfaceSubClass, desc.bInterfaceProtocol);
      loggDebug("n: %X, setting: %X", desc.bInterfaceNumber, desc.bAlternateSetting);
      for(int j = 0; j < interface->descriptor.bNumEndpoints; j++){
         UsbEndpointDescriptor *endpointDescriptor = &interface->endpoints[j];
         loggDebug("config %X", endpointDescriptor->bmAttributes);
         int status = configureEndpoint(xhcd, device->slotId, endpointDescriptor, &inputContext);
         if(status != XhcOk){
            loggError("Failed to confiure endpoint, status %X", status);
            return status;
         }
         loggInfo("Configured endpoint");
      }
   }
   XhcStatus status = runConfigureEndpointCommand(xhcd, device->slotId, &inputContext);
   if(status != XhcOk){
      loggError("Endpoint config error!");
      return status;
   }
   TD td = TD_SET_CONFIGURATION(configuration->descriptor.bConfigurationValue);
   if(!putConfigTD(xhcd, device->slotId, td)){
      loggError("Failed to set configuration");
      return XhcNotYetImplemented; //FIXME: Wrong error code
   }
   return XhcOk;
}
static XhcStatus configureEndpoint(Xhcd *xhcd, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext){
   switch(endpoint->transferType){
      case ENDPOINT_TRANSFER_TYPE_INTERRUPT:
         return initInterruptEndpoint(xhcd, slotId, endpoint, inputContext);
      case ENDPOINT_TRANSFER_TYPE_BULK:
         return initBulkEndpoint(xhcd, slotId, endpoint, inputContext);
      default:
         loggWarning("Transfer type not yet implemented %d", endpoint->transferType);
         return XhcNotYetImplemented;
   }
}
XhcStatus xhcd_readData(const XhcDevice *device, UsbEndpointDescriptor endpoint, void *dataBuffer, uint16_t bufferSize){
   loggDebug("Read Data\n");
   int endpointIndex = getEndpointIndex(&endpoint);

   Xhcd *xhcd = device->data;

   TRB trb = TRB_NORMAL(dataBuffer, bufferSize);
   XhcdRing *transferRing = xhcd->transferRing[device->slotId][endpointIndex - 1];
   xhcdRing_putTRB(transferRing, trb);
   xhcd_ringDoorbell(xhcd, device->slotId, endpointIndex);

   XhcEventTRB event;
   while(!dequeEventTrb(xhcd, &event));

   //FIXME: Need to handle errors
   loggDebug("Event ring trb type %d, status %d", event.trbType, event.completionCode);

   return XhcOk;
}
XhcStatus xhcd_writeData(const XhcDevice *device,
      UsbEndpointDescriptor endpoint,
      void *dataBuffer,
      uint16_t bufferSize){

   loggDebug("Write data");

   int endpointIndex = getEndpointIndex(&endpoint);
   Xhcd *xhcd = device->data;
   TRB trb = TRB_NORMAL(dataBuffer, bufferSize);
   XhcdRing *transferRing = xhcd->transferRing[device->slotId][endpointIndex - 1];
   xhcdRing_putTRB(transferRing, trb);
   xhcd_ringDoorbell(xhcd, device->slotId, endpointIndex);

   XhcEventTRB event;
   loggDebug("Waiting for event");
   while(!dequeEventTrb(xhcd, &event));
   loggDebug("Event ring trb type %d, status %d", event.trbType, event.completionCode);
   if(event.completionCode != Success){
      return XhcReadDataError;
   }
   return XhcOk;
}
XhcStatus xhcd_sendRequest(const XhcDevice *device, UsbRequestMessage request){
   logging_startContext("xhcd send request"){
      loggDebug("Send request");

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
      if(!putConfigTD(device->data, device->slotId, td)){
         lreturn XhcSendRequestError;
      }
   }
   return XhcOk;
}
/*
 * [ ]64-bit
 * [X] superspeed stuff p.179
 * [ ] Not enough power? look in configuration descriptor
 * [X] Get interface? Am i using the correct one?
 * [ ] p.101 SetAddress timing?
 * [ ] p.192 Direction of setup stage
 */
void xhc_dumpCapabilityRegs(Xhci *xhci){
   Xhcd *xhcd = xhci->data;
   kprintf("capability params (HCCPARAMS): ");
   kprintf("1: %X. 2: %X\n", xhcd_readCapability(xhcd->hardware, HCCPARAMS1), xhcd_readCapability(xhcd->hardware, HCCPARAMS2));
   kprintf("struct params (HCSPARAMS): ");
   kprintf("1: %X. 2: %X. 3: %X\n", xhcd_readCapability(xhcd->hardware, HCSPARAMS1),  xhcd_readCapability(xhcd->hardware, HCSPARAMS2), xhcd_readCapability(xhcd->hardware, HCSPARAMS3));
}

void xhc_dumpOperationalRegs(Xhci *xhci){
   Xhcd *xhcd = xhci->data;
   kprintf("USBCMD: %X\n", xhcd_readRegister(xhcd->hardware, USBCommand));
   kprintf("USBSTS: %X\n", xhcd_readRegister(xhcd->hardware, USBStatus));
   kprintf("pageSize: %X\n", xhcd_readRegister(xhcd->hardware, PAGESIZE));
   kprintf("DNCTRL: %X\n", xhcd_readRegister(xhcd->hardware, DNCTRL));
   kprintf("CRCR: %X %X\n", xhcd_readRegister(xhcd->hardware, CRCR));
   kprintf("DCBAAP: %X %X\n", xhcd_readRegister(xhcd->hardware, DCBAAP));
   kprintf("CONFIG: %X\n", xhcd_readRegister(xhcd->hardware, CONFIG));
}
static XhcStatus initInterruptEndpoint(Xhcd *xhcd, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext){
   loggDebug("Init endpoint. In? : %b", endpoint->direction == ENDPOINT_DIRECTION_IN);
   loggDebug("Addr: %X", endpoint->bEndpointAddress);
   int endpointIndex = getEndpointIndex(endpoint);

   XhcdRing *transferRing = xhcdRing_new(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhcd->transferRing[slotId][endpointIndex - 1] = transferRing;

   uint32_t maxPacketSize = endpoint->wMaxPacketSize & 0x7FF;
   uint32_t maxBurstSize = (endpoint->wMaxPacketSize & 0x1800) >> 11;
   uint32_t maxESITPayload = maxPacketSize * (maxBurstSize + 1);
   uint32_t endpointType =
      endpoint->direction == ENDPOINT_DIRECTION_IN ? ENDPOINT_TYPE_INTERRUPT_IN : ENDPOINT_TYPE_INTERRUPT_OUT;

   loggDebug("enpointType %d, max packet size: %X, max burst size: %X, maxESITPayload: %X, interval: %X ",
         endpointType, maxPacketSize, maxBurstSize, maxESITPayload, endpoint->bInterval);
   XhcEndpointContext *endpointContext = &inputContext->endpointContext[endpointIndex - 1];
   *endpointContext = (XhcEndpointContext){
      .endpointType = endpointType,
         .maxPacketSize = maxPacketSize,
         .maxBurstSize = maxBurstSize,
         .errorCount = 3,
         .dequeuePointer = (uintptr_t)transferRing->dequeue | transferRing->pcs, //X
         .maxESITPayloadLow = (uint16_t)maxESITPayload,
         .maxESITPayloadHigh = (uint16_t)(maxESITPayload >> 16),
         .interval = 6, //FIXME
         .avarageTrbLength = maxESITPayload,
   };
   inputContext->inputControlContext.addContextFlags |= 1 << endpointIndex;
   return XhcOk;
}
static XhcStatus initBulkEndpoint(Xhcd *xhcd, int slotId, UsbEndpointDescriptor *endpoint, XhcInputContext *inputContext){
   int endpointIndex = getEndpointIndex(endpoint);

   inputContext->inputControlContext.addContextFlags |= 1 << endpointIndex;

   uint32_t maxBurstSize = 0;
   uint32_t maxPrimaryStreams = 0;
   uintptr_t dequePointer = 0;
   uint32_t hostInitiateDisable = 0;
   uint32_t linearStreamArray = 0;

   XhcdRing *transferRing = xhcdRing_new(DEFAULT_TRANSFER_RING_TRB_COUNT);
   loggDebug("Creating bulk for index %d at %X", endpointIndex, transferRing->dequeue);
   xhcd->transferRing[slotId][endpointIndex - 1] = transferRing;
   dequePointer = (uintptr_t)transferRing->dequeue | transferRing->pcs;

   if(endpoint->superSpeedDescriptor){
      maxBurstSize = endpoint->superSpeedDescriptor->bMaxBurst;

      if(endpoint->superSpeedDescriptor->maxStreams > 0){
         loggWarning("Streams not yet implemented");
         return XhcNotYetImplemented;

      }
      else{
         maxPrimaryStreams = 0;
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

static int getSlotId(Xhcd *xhcd, uint8_t portNumber){
   loggDebug("Getting slot id");
   XhcEventTRB trb;

   xhcdRing_putTRB(xhcd->commandRing, TRB_ENABLE_SLOT(getProtocolSlotType(xhcd, portNumber)));
   ringCommandDoorbell(xhcd);
   //FIXME: hack
   loggDebug("waiting!");

   while(!dequeEventTrb(xhcd, &trb) || trb.trbType != CommandCompletionEvent);
   if(trb.completionCode == NoSlotsAvailiableError){
      loggWarning("No slots availiable");
      return -1;
   }
   if(trb.completionCode != Success){
      loggError("Something went wrong %d (initPort)", trb.completionCode);
      return -1;
   }
   loggInfo("Slot id %d", trb.slotId);
   return trb.slotId;
}

static PortSpeed getPortSpeed(Xhcd *xhcd, int portIndex){
   PortStatusAndControll portStatus = { .bits = xhcd_readPortRegister(xhcd->hardware, portIndex, PORTStatusAndControl) };
   PortSpeed speed = portStatus.portSpeed;
   loggDebug("Speed: %X", speed);
   return speed;
}
static PortUsbType getUsbType(Xhcd *xhcd, int portNumber){
   return xhcd->portInfo[portNumber].usbType;
}
static PortUsbType getProtocolSlotType(Xhcd *xhcd, int portNumber){
   return xhcd->portInfo[portNumber].protocolSlotType;
}
static void initDefaultInputContext(XhcInputContext *inputContext, int portIndex, XhcdRing* transferRing, PortSpeed speed){
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
         loggWarning("Unknown speed %X", speed);
         while(1);
   }

   XhcEndpointContext *controlEndpoint = &inputContext->endpointContext[0];
   controlEndpoint->endpointType = ENDPOINT_TYPE_CONTROL;
   controlEndpoint->maxPacketSize = maxPacketSize;
   controlEndpoint->dequeuePointer = (uintptr_t)transferRing->dequeue | transferRing->pcs;
   controlEndpoint->errorCount = 3;
   controlEndpoint->avarageTrbLength = 8;
}
static int addressDevice(Xhcd *xhcd, int slotId, int portIndex){
   XhcInputContext inputContext __attribute__((aligned(16)));
   memset((void*)&inputContext, 0, sizeof(XhcInputContext));

   loggDebug("Address device");
   XhcOutputContext *outputContext = kcallocco(sizeof(XhcOutputContext), 64, 0);
   xhcd->dcBaseAddressArray[slotId] = paging_getPhysicalAddress((uintptr_t)outputContext);

   XhcdRing *transferRing = xhcdRing_new(DEFAULT_TRANSFER_RING_TRB_COUNT);
   xhcd->transferRing[slotId][0] = transferRing;
   loggDebug("New ring");

   PortSpeed speed = getPortSpeed(xhcd, portIndex);
   initDefaultInputContext(&inputContext, portIndex, transferRing, speed);
   uintptr_t inputContextPhysical = paging_getPhysicalAddress((uintptr_t)&inputContext);
   xhcdRing_putTRB(xhcd->commandRing, TRB_ADDRESS_DEVICE(inputContextPhysical, slotId, 0));
   ringCommandDoorbell(xhcd);

   loggDebug("init context (waiting)");
   XhcEventTRB result;
   while(dequeEventTrb(xhcd, &result) == 0);
   if(result.completionCode != Success){
      loggError("Failed to addres device (Event: %X %X %X %X, code: %d)", result, result.completionCode);
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
   loggInfo("Successfully addressed device: (Event: %X %X %X %X)", result);
   return 1;
}
static int checkoutPort(Xhcd *xhcd, int portIndex){
   PortStatusAndControll status = { .bits = xhcd_readPortRegister(xhcd->hardware, portIndex, PORTStatusAndControl) };
   if(status.connectStatusChange && status.currentConnectStatus){
      status.connectStatusChange = 1;
      xhcd_writePortRegister(xhcd->hardware, portIndex, PORTStatusAndControl, status.bits);
      return 1;
   }
   return 0;
}
static XhcStatus resetXhc(Xhcd *xhcd){
   loggDebug("Reset xhc");

   xhcd_andRegister(xhcd->hardware, USBCommand, ~1);
   if(!RETRY_WITH_DELAY((xhcd_readRegister(xhcd->hardware, USBStatus) & 1), 10, 100)){
      return XhcControllerTimedOut;
   }
   xhcd_orRegister(xhcd->hardware, USBCommand, 1 << 1);
   if(!RETRY_WITH_DELAY(((xhcd_readRegister(xhcd->hardware, USBCommand) & (1 << 1)) == 0), 10, 100)){
      return XhcControllerTimedOut;
   }

   return XhcOk;
}
static XhcStatus initCommandRing(Xhcd *xhcd){
   xhcd->commandRing = xhcdRing_new(DEFAULT_COMMAND_RING_SIZE);
   if(!xhcd->commandRing){
      return XhcUnableToAllocateMemory;
   }
   xhcd_attachCommandRing(xhcd->hardware, xhcd->commandRing);
   return XhcOk;
}
//FIXME: interrupter register
static XhcStatus initEventRing(Xhcd *xhcd){
   size_t maxTrbCount = xhcdEventRing_getMaxTrbCount(xhcd->hardware);
   size_t trbCount = min(DEFAULT_EVENT_RING_TRB_COUNT, maxTrbCount);
   loggDebug("Creating event ring of size %d/%d", trbCount, maxTrbCount);
   xhcd->eventRing = xhcdEventRing_new(trbCount);
   if(!xhcd->eventRing){
      return XhcUnableToAllocateMemory;
   }
   //Enable interrupt for interruptor
   // FIXME:check
   xhcd_orInterrupter(xhcd->hardware, 0, IMAN, 2);

   xhcdEventRing_attach(xhcd->hardware, xhcd->eventRing, 0);

   return XhcOk;
}
static int enablePort(Xhcd *xhcd, int portIndex){
   if(isPortEnabled(xhcd, portIndex)){
      loggWarning("Port already enabled");
      return 1;
   }
   PortUsbType usbType = getUsbType(xhcd, portIndex + 1);
   if(usbType == PortUsbType3){
      loggWarning("(USB3) port should already be enabled, index: %X", portIndex);
      return 0;
   }
   /*   if(!shouldEnablePort(xhci, portNumber)){ //FIXME: This should worrk
        printf("Failed to enable port (USB3)\n");
        return 0;
        }*/
   loggDebug("Enabling port (USB2) %X", portIndex);
   PortStatusAndControll temp = { .bits = xhcd_readPortRegister(xhcd->hardware, portIndex, PORTStatusAndControl) };//FIXME: is intex correct?

   temp.portEnabledDisabled = 0;
   temp.connectStatusChange = 0;
   temp.portEnableDisableChange = 0;
   temp.warmPortResetChange = 0;
   temp.overCurrentChange = 0;
   temp.portResetChange = 0;
   temp.portLinkStateChange = 0;
   temp.portConfigErrorChange = 0;

   temp.portReset = 1;

   xhcd_writePortRegister(xhcd->hardware, portIndex, PORTStatusAndControl, temp.bits);

   while(!temp.portResetChange){
      temp = (PortStatusAndControll){ .bits = xhcd_readPortRegister(xhcd->hardware, portIndex, PORTStatusAndControl) };//FIXME: is intex correct?
   }
   while(!temp.portEnabledDisabled){
      temp = (PortStatusAndControll){ .bits = xhcd_readPortRegister(xhcd->hardware, portIndex, PORTStatusAndControl) };//FIXME: is intex correct?
   }
   int isEnabled = isPortEnabled(xhcd, portIndex);

   temp.portEnabledDisabled = 0;
   temp.portReset = 0;
   temp.connectStatusChange = 0;
   temp.portEnableDisableChange = 0;
   temp.warmPortResetChange = 0;
   temp.overCurrentChange = 0;
   temp.portLinkStateChange = 0;
   temp.portConfigErrorChange = 0;

   temp.portResetChange = 1; //Clear
   xhcd_writePortRegister(xhcd->hardware, portIndex, PORTStatusAndControl, temp.bits);
   if(!isEnabled){
      return 0;
   }

   return 1;
}
static int runCommand(Xhcd *xhcd, TRB trb){
   xhcdRing_putTRB(xhcd->commandRing, trb);
   ringCommandDoorbell(xhcd);

   XhcEventTRB result;
   while(!dequeEventTrb(xhcd, &result));
   if(result.completionCode != Success){
      return 0;
   }
   return 1;
}
static XhcStatus runConfigureEndpointCommand(Xhcd *xhcd, int slotId, XhcInputContext *inputContext){
   XhcOutputContext *output = getOutputContext(xhcd, slotId);
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

   uintptr_t physicalInputContextAddress = paging_getPhysicalAddress((uintptr_t)inputContext);
   TRB trb = TRB_CONFIGURE_ENDPOINT((void*)physicalInputContextAddress, slotId);
   if(!runCommand(xhcd, trb)){
      loggError("Failed to configure endpoint (slotid: %d)", slotId);
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
static XhcOutputContext *getOutputContext(Xhcd *xhcd, int slotId){
   volatile uint64_t *dcAddressArray = xhcd->dcBaseAddressArray;
   uintptr_t ptr = dcAddressArray[slotId];
   XhcOutputContext *outputContext = (XhcOutputContext*)ptr;
   return outputContext;
}

static int putConfigTD(Xhcd *xhcd, int slotId, TD td){
   XhcEventTRB event;
   //    while(dequeEventTrb(xhcd, &event)); //FIXME: Hack

   XhcdRing *transferRing = xhcd->transferRing[slotId][0];
   xhcdRing_putTD(transferRing, td);
   xhcd_ringDoorbell(xhcd, slotId, 1);

   while(!dequeEventTrb(xhcd, &event));
   if(event.completionCode != Success){
      return 0;
   }
   return 1;
}

// static int setMaxPacketSize(Xhcd *xhcd, int slotId){
//    uint8_t buffer[8];
//    XhcdRing *transferRing = xhcd->transferRing[slotId][0];
//    xhcdRing_putTD(transferRing, TD_GET_DESCRIPTOR(buffer, sizeof(buffer)));
//    xhcd_ringDoorbell(xhcd, slotId, 1);

//    XhcEventTRB result;
//    while(!dequeEventTrb(xhcd, &result));
//    if(result.completionCode != Success){
//       loggError("Failed to get max packet size");
//       return 0;
//    }
//    uint8_t maxPacketSize = buffer[7];

//    uintptr_t address = xhcd->dcBaseAddressArray[slotId];
//    XhcOutputContext *output = (XhcOutputContext*)address;
//    uint8_t currMaxPacketSize = output->endpointContext[0].maxPacketSize;

//    if(maxPacketSize != currMaxPacketSize){
//       XhcInputContext *input = &inputContext[slotId];
//       input->endpointContext[0] = output->endpointContext[0];
//       input->endpointContext[0].maxPacketSize = maxPacketSize;
//       memset((void*)&input->inputControlContext, 0, sizeof(XhcInputControlContext));
//       input->inputControlContext.addContextFlags = 1 << 1;
//       loggDebug("add context %X", input->inputControlContext.addContextFlags);
//       xhcdRing_putTRB(xhcd->commandRing, TRB_EVALUATE_CONTEXT((void*)input, slotId));
//       ringCommandDoorbell(xhcd);
//       XhcEventTRB result;
//       while(!dequeEventTrb(xhcd, &result));
//       if(result.completionCode != Success){
//          loggError("Failed to set max packet size");
//          return 0;
//       }
//    }
//    currMaxPacketSize = output->endpointContext[0].maxPacketSize;
//    loggInfo("Sucessfully set max packet size: %d", currMaxPacketSize);
//    return 1;
// }

static int isPortEnabled(Xhcd *xhcd, int portIndex){
   PortStatusAndControll status = { .bits = xhcd_readPortRegister(xhcd->hardware, portIndex, PORTStatusAndControl) };
   if(status.portEnabledDisabled == 1
         && status.portReset == 0
         && status.portLinkState == 0){
      return 1;
   }
   return 0;
}
static void ringCommandDoorbell(Xhcd *xhcd){
   xhcd_writeDoorbell(xhcd->hardware, 0, 0);
}
static void xhcd_ringDoorbell(Xhcd *xhcd, uint8_t slotId, uint8_t target){
   if(slotId == 0){
      loggWarning("Unable to ring doorbell. Invalid slotId: 0");
      return;
   }
   if(target < 1){
      loggWarning("Unable to ring doorbell. Invalid target: 0");
      return;
   }
   xhcd_writeDoorbell(xhcd->hardware, slotId, target);
}
// static PortStatusAndControll *getPortStatus(Xhci *xhci, int portIndex){
//    XhciOperation *operation = xhci->operation;
//    XhciPortRegisters *port = &operation->ports[portIndex];
//    PortStatusAndControll *status = &port->statusAndControll;
//    return status;
// }
static int doBiosHandoff(Xhcd *xhcd){
   XhcExtendedCapabilityEnumerator enumerator = xhcd_newExtendedCapabilityEnumerator(xhcd->hardware);
   while(xhcd_hasNextExtendedCapability(&enumerator)){
      uint64_t capability;
      xhcd_readExtendedCapability(&enumerator, &capability, 4);
      if((capability & 0xFF) == 1){
         capability |= 1 << 24;

         xhcd_writeExtendedCapability(&enumerator, &capability, 4);

         do{
            xhcd_readExtendedCapability(&enumerator, &capability, 4);
         }
         while((capability & (1 << 24)) == 0 || (capability & (1 << 16)));

         loggInfo("Took control from bios");

         capability &= 0xFFFFFFFF;
         xhcd_writeExtendedCapability(&enumerator, &capability, 8);

         return 0;
      }
      xhcd_advanceExtendedCapabilityEnumerator(&enumerator);
   }

   return 0;
}
static XhcStatus waitForControllerReady(Xhcd *xhcd){
   loggDebug("Wait for controller ready");
   return RETRY_WITH_DELAY(((xhcd_readRegister(xhcd->hardware, USBStatus) & CNR_FLAG) == 0), 10, 100) ? XhcOk : XhcControllerTimedOut;
}
static int getMaxEnabledDeviceSlots(Xhcd *xhcd){
   StructParams1 structParams1 = {.bits = xhcd_readCapability(xhcd->hardware, HCSPARAMS1) };
   return structParams1.maxPorts;
}
static XhcStatus setMaxEnabledDeviceSlots(Xhcd *xhcd, uint8_t maxSlots){
   assert((xhcd_readRegister(xhcd->hardware, USBCommand) & USBCMD_RUN_STOP_BIT) == 0);

   xhcd->enabledPorts = maxSlots;

   xhcd_andRegister(xhcd->hardware, CONFIG, ~0xFF);
   xhcd_orRegister(xhcd->hardware, CONFIG, maxSlots);

   loggInfo("MaxSlotsEn: %X", maxSlots);

   return XhcOk;
}
static uint32_t getPageSize(Xhcd *xhcd){
   return (xhcd_readRegister(xhcd->hardware, PAGESIZE) & 0xFF) << 12;
}
static XhcStatus initDCAddressArray(Xhcd *xhcd){
   assert((xhcd_readRegister(xhcd->hardware, USBCommand) & USBCMD_RUN_STOP_BIT) == 0);

   XhcConfigRegister config = { .bits = xhcd_readRegister(xhcd->hardware, CONFIG) };
   uint8_t maxSlots = config.enabledDeviceSlots;
   uint32_t pageSize = getPageSize(xhcd);
   uint32_t arraySize = (maxSlots + 1) * sizeof(uint64_t);
   xhcd->dcBaseAddressArray = dma_kmallocco(arraySize, 64, pageSize);
   xhcd->dcBaseAddressArraySize = maxSlots + 1;
   if(!xhcd->dcBaseAddressArray){
      return XhcUnableToAllocateMemory;
   }
   memset((void *)xhcd->dcBaseAddressArray, 0, arraySize);
   uintptr_t dcAddressArrayPointer = paging_getPhysicalAddress((uintptr_t)xhcd->dcBaseAddressArray);
   xhcd_writeRegister(xhcd->hardware, DCBAAP, dcAddressArrayPointer);

   assert_physical_continguous(dcAddressArrayPointer, arraySize);
   return XhcOk;
}

static XhcStatus initScratchPad(Xhcd *xhcd){
   loggDebug("Init scrachpad");
   assert((xhcd_readRegister(xhcd->hardware, USBCommand) & USBCMD_RUN_STOP_BIT) == 0);
   assert(xhcd->dcBaseAddressArray != 0);
   assert(xhcd->scratchpadArray == 0);
   assert(xhcd->scratchpadPointers == 0);

   size_t scratchpadSize = getScratchpadSize(xhcd);
   size_t pageSize = getPageSize(xhcd);
   loggDebug("Required scratchpad size %d", scratchpadSize);
   loggDebug("Page size: %d", pageSize);

   xhcd->scratchpadArray = dma_kmallocco(scratchpadSize * sizeof(uint64_t), 64, pageSize);
   xhcd->scratchpadPointers = kmalloc(scratchpadSize * sizeof(void *));
   if(!xhcd->scratchpadArray || !xhcd->scratchpadPointers){
      return XhcUnableToAllocateMemory;
   }

   memset(xhcd->scratchpadArray, 0, scratchpadSize * sizeof(uint64_t));
   memset(xhcd->scratchpadPointers, 0, scratchpadSize * sizeof(void *));

   for(size_t i = 0; i < scratchpadSize; i++){
      void* scratchpadStart = dma_kmallocco(pageSize, 1, pageSize);
      if(!scratchpadStart){
         return XhcUnableToAllocateMemory;
      }

      memset(scratchpadStart, 0, pageSize);

      xhcd->scratchpadArray[i] = (uintptr_t)paging_getPhysicalAddress((uintptr_t)scratchpadStart);
      xhcd->scratchpadPointers[i] = scratchpadStart;
   }

   xhcd->dcBaseAddressArray[0] = (uint64_t)paging_getPhysicalAddress((uintptr_t)xhcd->scratchpadArray);

   loggInfo("Initialized xHCI scratchpad array: virt=%X phys=%X",
         xhcd->scratchpadArray,
         paging_getPhysicalAddress((uintptr_t)xhcd->scratchpadArray));

   return XhcOk;
}

static uint32_t getScratchpadSize(Xhcd *xhcd){
   StructParams2 structParams2 = { .bits = xhcd_readCapability(xhcd->hardware, HCSPARAMS2) };
   uint32_t scratchpadSize = structParams2.maxScratchpadBuffersHigh << 5;
   scratchpadSize |= structParams2.maxScratchpadBuffersLow;
   return scratchpadSize;
}

static void turnOnController(Xhcd *xhcd){
   xhcd_orRegister(xhcd->hardware, USBCommand, USBCMD_RUN_STOP_BIT);
}

static bool assert_physical_continguous(uintptr_t start, size_t length){
   #ifdef ASSERTS_ENABLED
      uintptr_t lastAddress = paging_getPhysicalAddress((uintptr_t)start);
      for(size_t i = sizeof(uint32_t); i < length; i += sizeof(uint32_t)){
         uintptr_t currAddress = paging_getPhysicalAddress((uintptr_t)start + i);
         if(!assert(lastAddress + sizeof(uint32_t) == currAddress)){
            return false;
         }
         lastAddress = currAddress;
      } 

   #endif
   return true;
}
