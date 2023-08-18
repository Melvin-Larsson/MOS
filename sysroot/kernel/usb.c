#include "kernel/usb.h"
#include "kernel/xhcd-ring.h"
#include "kernel/xhcd-event-ring.h"

static UsbStatus initInterface(
      UsbDevice2 *device,
      UsbInterface *interface);
static UsbStatus initEndpoint(
      UsbDevice2 *device,
      UsbEndpointDescriptor *endpoint);

UsbStatus usb_setConfiguration(UsbDevice2 *device, UsbConfiguration *configuration){
   Xhci *xhci = device->usb->xhci;
   if(!xhcd_setConfiguration(xhci, device->slotId, configuration)){
      return StatusError;
   }
   for(int i = 0; i < configuration->descriptor.bNumInterfaces; i++){
      UsbStatus status = initInterface(device, &configuration->interfaces[i]);
      if(status != StatusSuccess){
         return status;
      }
   }
   return StatusSuccess;
}
UsbStatus usb_configureDevice(UsbDevice2 *device, DeviceConfigTransfer config){
   SetupStageHeader header;
   header.bmRequestType = config.bmRequestType;
   header.bRequest = config.bRequest;
   header.wValue = config.wValue;
   header.wIndex = config.wIndex;
   header.wLength = config.wLength;
   TRB setupTrb = TRB_SETUP_STAGE(header);
   TRB statusTrb = TRB_STATUS_STAGE();

   TD td;
   if(header.wLength == 0){
      td = (TD){{setupTrb, statusTrb}, 2};
   }else{
      TRB dataTrb = TRB_DATA_STAGE(config.dataBuffer, config.wLength);
      td = (TD){{setupTrb, dataTrb, statusTrb}, 3};
   }
   Xhci *xhci = device->usb->xhci;
   xhcd_putTD(td, &xhci->transferRing[device->slotId][0]);
   xhcd_ringDoorbell(xhci, device->slotId, 1);

   XhcEventTRB result;
   while(!xhcd_readEvent(&xhci->eventRing, &result, 1));
   if(result.completionCode != Success){
      return StatusError;
   }
   return StatusSuccess;
}
UsbStatus usb_readData(UsbDevice2 *device, int endpoint, void *dataBuffer, int dataBufferSize){
   Xhci *xhci = device->usb->xhci;
   if(!xhcd_readData(xhci, device->slotId, endpoint, dataBuffer, dataBufferSize)){
      return StatusError;
   }
   return StatusSuccess;

}
static UsbStatus initInterface(UsbDevice2 *device, UsbInterface *interface){
   for(int i = 0; i < interface->descriptor.bNumEndpoints; i++){
      UsbStatus status = initEndpoint(device, &interface->endpoints[i]);
      if(status != StatusSuccess){
         return status;
      }
   }
   return StatusSuccess;
}

static UsbStatus initEndpoint(UsbDevice2 *device, UsbEndpointDescriptor *endpoint){
   if(!xhcd_configureEndpoint(device->usb->xhci, device->slotId, endpoint)){
      return StatusError;
   }
   return StatusSuccess;
}

