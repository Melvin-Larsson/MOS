#include "kernel/usb.h"
#include "kernel/xhcd-ring.h"
#include "kernel/xhcd-event-ring.h"
#include "kernel/usb-messages.h"
#include "stdlib.h"
#include "stdio.h"

#define DESCRIPTOR_TYPE_DEVICE 1
#define DESCRIPTOR_TYPE_CONFIGURATION 2
#define DESCRIPTOR_TYPE_INTERFACE 4
#define DESCRIPTOR_TYPE_ENDPOINT 5

#define REQUEST_SET_CONFIGURATION 9 
#define REQUEST_GET_DESCRIPTOR 6
#define DESCRIPTOR_TYPE_DEVICE 1 

static UsbDevice2 initUsbDevice(Usb *usb, const XhcDevice *device);

static UsbStatus getDeviceDescriptor(Usb *usb, const XhcDevice *device, UsbDeviceDescriptor *result); 
static UsbStatus getConfiguration(Usb *usb, const XhcDevice *device, int configuration, UsbConfiguration **result); 
static UsbConfiguration *parseConfiguration(uint8_t *configBuffer);
static void freeConfiguration(UsbConfiguration *config);
static void freeInterface(UsbInterface *interface);

UsbStatus usb_init(PciGeneralDeviceHeader *pci, Usb *result){
   if(pci->pciHeader.classCode != PCI_CLASS_SERIAL_BUS_CONTROLLER){
      return StatusError;
   }
   if(pci->pciHeader.subclass != PCI_SUBCLASS_USB_CONTROLLER){
      return StatusError;
   }
   if(pci->pciHeader.progIf == PCI_PROG_IF_XHCI){
      Xhci *xhci = malloc(sizeof(Xhci));
      if(xhcd_init(pci, xhci) != XhcOk){
         free(xhci);
         return StatusError;
      }
      *result  = (Usb){xhci};
      return StatusSuccess;
   }
   printf("USB controller not yet implemented\n");
   return StatusError;
}
int usb_getNewlyAttachedDevices(Usb *usb, UsbDevice2 *resultBuffer, int bufferSize){
   XhcDevice *deviceBuffer = malloc(bufferSize * sizeof(XhcDevice));
   int attachedPortsCount = xhcd_getDevices(usb->xhci, deviceBuffer, bufferSize);
   for(int i = 0; i < attachedPortsCount; i++){
      resultBuffer[i] = initUsbDevice(usb, &deviceBuffer[i]);
   }
   free(deviceBuffer);
   return attachedPortsCount;
}

UsbStatus usb_setConfiguration(UsbDevice2 *device, UsbConfiguration *configuration){
   if(xhcd_setConfiguration(device->usb->xhci, device->xhcDevice, configuration) != XhcOk){
      return StatusError;
   }
   return StatusSuccess;
}
UsbStatus usb_configureDevice(UsbDevice2 *device, UsbRequestMessage message){
   if(xhcd_sendRequest(device->usb->xhci, device->xhcDevice, message) != XhcOk){
      return StatusError;
   }
   return StatusSuccess;
}
UsbDeviceDescriptor usb_getDeviceDescriptor(UsbDevice2 *device){
   return device->deviceDescriptor;
}
UsbStatus usb_readData(UsbDevice2 *device, int endpoint, void *dataBuffer, int dataBufferSize){
   Xhci *xhci = device->usb->xhci;
   if(xhcd_readData(xhci, device->xhcDevice, endpoint, dataBuffer, dataBufferSize) != XhcOk){
      return StatusError;
   }
   return StatusSuccess;

}
static UsbDevice2 initUsbDevice(Usb *usb, const XhcDevice *device){
   UsbDeviceDescriptor descriptor;
   getDeviceDescriptor(usb, device, &descriptor);

   int configCount = descriptor.bNumConfigurations;
   UsbConfiguration *configurations = malloc(sizeof(UsbConfiguration) * configCount);
   for(int j = 0; j < configCount; j++){
      UsbConfiguration *config;
      getConfiguration(usb, device, j, &config);
      configurations[j] = *config;
      free(config);
   }
   XhcDevice *xhcDevice = malloc(sizeof(XhcDevice));
   *xhcDevice = *device;
   UsbDevice2 usbDevice = {xhcDevice, descriptor, configurations, configCount, usb};
   return usbDevice;
}
static UsbStatus getDeviceDescriptor(Usb *usb, const XhcDevice *device, UsbDeviceDescriptor *result){
   UsbRequestMessage request;
   request.bmRequestType = 0x80;
   request.bRequest = REQUEST_GET_DESCRIPTOR;
   request.wValue = DESCRIPTOR_TYPE_DEVICE << 8;
   request.wIndex = 0;
   request.wLength = sizeof(UsbDeviceDescriptor);
   request.dataBuffer = result;

   if(xhcd_sendRequest(usb->xhci, device, request) != XhcOk){
      return StatusError;
   }
   return StatusSuccess;

}
static UsbStatus getConfiguration(Usb *usb, const XhcDevice *device, int configuration, UsbConfiguration **result){
   const int bufferSize =
      sizeof(UsbConfigurationDescriptor) +
      sizeof(UsbInterfaceDescriptor) * 32 +
      sizeof(UsbEndpointDescriptor) * 32 * 15;
   uint8_t buffer[bufferSize];

   UsbRequestMessage request;
   request.bmRequestType = 0x80;
   request.bRequest = REQUEST_GET_DESCRIPTOR;
   request.wValue = DESCRIPTOR_TYPE_CONFIGURATION << 8 | configuration;
   request.wIndex = 0;
   request.wLength = sizeof(buffer);
   request.dataBuffer = buffer;
   if(xhcd_sendRequest(usb->xhci, device, request) != XhcOk){
      return StatusError;
   }
   *result = parseConfiguration(buffer);
   return StatusSuccess;
}
static UsbConfiguration *parseConfiguration(uint8_t *configBuffer){
   uint8_t *pos = configBuffer;
   UsbConfiguration *config = malloc(sizeof(UsbConfiguration));
   UsbConfigurationDescriptor *configDescriptor = (UsbConfigurationDescriptor*)configBuffer;
   config->descriptor = *configDescriptor;
   config->interfaces = malloc(sizeof(UsbInterface) * configDescriptor->bNumInterfaces);
   pos += sizeof(UsbConfigurationDescriptor);

   for(int i = 0; i < configDescriptor->bNumInterfaces; i++){
      UsbInterfaceDescriptor *interfaceDescriptor = (UsbInterfaceDescriptor*)pos;
      UsbInterface *interface = &config->interfaces[i];
      interface->descriptor = *interfaceDescriptor;
      interface->endpoints = malloc(sizeof(UsbEndpointDescriptor) * interfaceDescriptor->bNumEndpoints);
      pos += sizeof(UsbInterfaceDescriptor);

      for(int j = 0; j < interfaceDescriptor->bNumEndpoints; j++){
         UsbEndpointDescriptor *endpointDescriptor = (UsbEndpointDescriptor*)pos;
         if(endpointDescriptor->bDescriptorType != DESCRIPTOR_TYPE_ENDPOINT){ //FIXME: kind of a hack to ignore HID descriptors
            j--;
            pos += endpointDescriptor->bLength;
         }else{
            UsbEndpointDescriptor *endpoint = &interface->endpoints[j];
            *endpoint = *endpointDescriptor;
            pos += sizeof(UsbEndpointDescriptor);
         }
      }
   }
   return config;
}
static void freeConfiguration(UsbConfiguration *config){
   for(int i = 0; i < config->descriptor.bNumInterfaces; i++){
      freeInterface((void*)&config->interfaces[i]);
   }
   free(config);
}
static void freeInterface(UsbInterface *interface){
   for(int i = 0; i < interface->descriptor.bNumEndpoints; i++){
      free((void*)&interface->endpoints[i]);
   }
   free(interface);
}
