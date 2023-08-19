#ifndef USB_H_INCLUDED
#define USB_H_INCLUDED

#include "usb-descriptors.h"
#include "usb-messages.h"
#include "xhcd.h"
#include "pci.h"

typedef struct{
   Xhci *xhci;
}Usb;

typedef struct{
   int slotId;
   UsbDeviceDescriptor deviceDescriptor;
   UsbConfiguration *configuration;
   int configurationCount;
   Usb* usb;
}UsbDevice2;

typedef enum{
   StatusSuccess = 0,
   StatusError = 1
}UsbStatus;

UsbStatus usb_init(PciGeneralDeviceHeader *pci, Usb *result);
int usb_getNewlyAttachedDevices(Usb *usb, UsbDevice2 *resultBuffer, int bufferSize);

UsbStatus usb_setConfiguration(
      UsbDevice2 *device,
      UsbConfiguration *configuration);
UsbDeviceDescriptor usb_getDeviceDescriptor(UsbDevice2 *deviceor);
UsbStatus usb_clearFeature(UsbDevice2 *device, uint16_t featureSelector);
UsbStatus usb_getConfiguration(UsbDevice2 *device, uint8_t result[1]);
UsbStatus usb_getDescriptor(UsbDevice2 *device, void *buffer, uint16_t bufferSize);
UsbStatus usb_getInterface(UsbDevice2 *device, uint16_t interface, uint8_t *result);
UsbStatus usb_getStatus(UsbDevice2 *device, uint8_t statusType, uint16_t index, uint8_t result[2]);
UsbStatus usb_setAddress(UsbDevice2 *device, uint16_t address);
UsbStatus usb_setFeature(UsbDevice2 *device, uint16_t feature, uint16_t index);
UsbStatus usb_setInterface(UsbDevice2 *device, uint16_t alternateSetting, uint16_t interface);
UsbStatus usb_setIsochDelay(UsbDevice2 *device, uint16_t delay);
UsbStatus usb_setSel(UsbDevice2 *devive, uint8_t values[6]);
UsbStatus usb_syncFrame(UsbDevice2 *device, uint16_t endpoint, uint8_t frameNumber[2]);

UsbStatus usb_configureDevice(UsbDevice2 *device, UsbRequestMessage message);

UsbStatus usb_readData(UsbDevice2 *device, int endpoint, void *dataBuffer, int dataBufferSize);


#endif
