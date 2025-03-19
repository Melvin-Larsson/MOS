#ifndef USB_MASS_STORAGE_H_INCLUDED
#define USB_MASS_STORAGE_H_INCLUDED

#include "kernel/usb.h"

typedef enum{
   UsbMassStorageSuccess,
   UsbMassStorageInvalidDevice,
   UsbMassStorageConfigError,
   UsbMassStorageProtocolError,
   UsbMassStorageCommandFailed,
   UsbMassStoragePhaseError,
   UsbMassStorageInvalidAddress,
   UsbMassStorageUnexpectedMessage,
}UsbMassStorageStatus;

typedef struct{
   UsbDevice *usbDevice;
   UsbConfiguration *configuration;
   UsbInterface *interface;
   UsbEndpointDescriptor bulkOutEndpoint;
   UsbEndpointDescriptor bulkInEndpoint;
   uint32_t capacity;
   uint32_t maxLogicalBlockAddress;
   uint8_t inquiryData[36];
}UsbMassStorageDevice;

UsbMassStorageStatus usbMassStorage_init(UsbDevice *usbDevice,
      UsbMassStorageDevice *result);
UsbMassStorageStatus usbMassStorage_read(const UsbMassStorageDevice *device,
      uint32_t logicalBlockAddress,
      void *resultBuffer,
      uint32_t bufferSize);
UsbMassStorageStatus usbMassStorage_write(const UsbMassStorageDevice *device,
      uint32_t logicalBlockAddress,
      void *data,
      uint32_t dataSize);

#endif
