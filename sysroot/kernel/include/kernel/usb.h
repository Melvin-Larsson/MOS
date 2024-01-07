#ifndef USB_H_INCLUDED
#define USB_H_INCLUDED

#include "usb-descriptors.h"
#include "usb-messages.h"
#include "xhcd.h"
#include "pci.h"

typedef enum{
   UsbControllerXhci,
}UsbController;

typedef struct{
   UsbController type;
   union{
      Xhci *xhci;
   };
}Usb;

typedef struct{
   UsbController type;
   union{
      XhcDevice  *xhcDevice;
   };

}UsbControllerDevice;

typedef struct{
   UsbControllerDevice controllerDevice;
   UsbDeviceDescriptor deviceDescriptor;
   UsbConfiguration *configuration;
   int configurationCount;
   Usb* usb;
}UsbDevice;

typedef enum{
   RecipientDevice = 0,
   RecipientInterface = 1,
   RecipientEndpoint = 2,
}Recipient;

typedef enum{
   StatusTypeStandard = 0,
   StatusTypePtm = 1,
}StatusType;

typedef enum{
   StatusSuccess = 0,
   StatusError = 1
}UsbStatus;

UsbStatus usb_init(PciGeneralDeviceHeader *pci, Usb *result);
int usb_getNewlyAttachedDevices(Usb *usb, UsbDevice *resultBuffer, int bufferSize);

UsbStatus usb_getStatus(UsbDevice *device, Recipient recipient, StatusType statusType, uint16_t index, uint8_t result[2]);

//TODO: implement
void usb_freeUsbDevice(UsbDevice *device);

UsbStatus usb_setConfiguration(
      UsbDevice *device,
      UsbConfiguration *configuration);
UsbDeviceDescriptor usb_getDeviceDescriptor(UsbDevice *deviceor);
//TODO: implement
UsbStatus usb_clearFeature(UsbDevice *device, uint16_t featureSelector);
UsbStatus usb_getConfiguration(UsbDevice *device, uint8_t result[1]);
UsbStatus usb_getDescriptor(UsbDevice *device, void *buffer, uint16_t bufferSize);
UsbStatus usb_getInterface(UsbDevice *device, uint16_t interface, uint8_t *result);
UsbStatus usb_setAddress(UsbDevice *device, uint16_t address);
UsbStatus usb_setFeature(UsbDevice *device, uint16_t feature, uint16_t index);
UsbStatus usb_setInterface(UsbDevice *device, uint16_t alternateSetting, uint16_t interface);
UsbStatus usb_setIsochDelay(UsbDevice *device, uint16_t delay);
UsbStatus usb_setSel(UsbDevice *devive, uint8_t values[6]);
UsbStatus usb_syncFrame(UsbDevice *device, uint16_t endpoint, uint8_t frameNumber[2]);

UsbStatus usb_configureDevice(UsbDevice *device, UsbRequestMessage message);

UsbStatus usb_readData(UsbDevice *device, UsbEndpointDescriptor endpoint, void *dataBuffer, int dataBufferSize);
UsbStatus usb_writeData(UsbDevice *device, UsbEndpointDescriptor endpoint, void *dataBuffer, int dataBufferSize);


#endif
