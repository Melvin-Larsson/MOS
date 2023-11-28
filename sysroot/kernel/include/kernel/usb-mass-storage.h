#ifndef USB_MASS_STORAGE_H_INCLUDED
#define USB_MASS_STORAGE_H_INCLUDED

#include "kernel/usb.h"

typedef enum{
   UsbMassStorageSuccess,
   UsbMassStorageInvalidDevice,
   UsbMassStorageConfigError,
   UsbMassStorageProtocolError,
}UsbMassStorageStatus;

typedef struct{
   UsbDevice *usbDevice;
   UsbConfiguration *configuration;
   UsbInterface *interface;
}UsbMassStorageDevice;

UsbMassStorageStatus usbMassStorage_init(UsbDevice *usbDevice, UsbMassStorageDevice *result);


#endif
