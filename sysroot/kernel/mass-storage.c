#include "kernel/mass-storage.h"

int massStorageDevice_initUsb(UsbMassStorageDevice *device,
      MassStorageDevice *result){

   *result = (MassStorageDevice){
      .type = MassStorageDeviceUsb,
      .blockSize = device->capacity,
      .usbMassStorage = device,
      .read = (int(*)(void*, uint32_t, void*, uint32_t))usbMassStorage_read, //FIXME: void* const
      .write = (int(*)(void*, uint32_t, void*, uint32_t))usbMassStorage_write,
   };

   return 0;
}
