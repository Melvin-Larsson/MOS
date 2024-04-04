#ifndef MASS_STORAGE_H_INCLUDED
#define MASS_STORAGE_H_INCLUDED

#include "usb-mass-storage.h"

typedef enum{
   MassStorageDeviceUsb,
}MassStorageDeviceType;

typedef struct{
   MassStorageDeviceType type;
   uint32_t blockSize;
   union{
      UsbMassStorageDevice *usbMassStorage;
      void *data;
   };
   int (*read)(void *device,
            uint32_t logicalBlockAddress,
            void *result,
            uint32_t bufferSize);
   int (*write)(void *device,
            uint32_t logicalBlockAddress,
            void *data,
            uint32_t dataSize);
}MassStorageDevice;

int massStorageDevice_initUsb(UsbMassStorageDevice *device,
                              MassStorageDevice *result);


#endif

