#include "stdint.h"
#include "stdlib.h"
#include "mass-storage-device-mock.h"
#include "stdio.h"

static uint32_t  _dataSize;


int read(void *d,
         uint32_t logicalBlockAddress,
         void *result,
         uint32_t bufferSize){

   MassStorageDevice *device = (MassStorageDevice*)d;

   uint32_t dataToRead = bufferSize;
   if(logicalBlockAddress * device->blockSize + bufferSize > _dataSize){
      dataToRead = _dataSize - logicalBlockAddress * device->blockSize;
   }

   memcpy(result, device->data + logicalBlockAddress * device->blockSize, dataToRead);

   return dataToRead;
}
int write(void *d,
         uint32_t logicalBlockAddress,
         void *data,
         uint32_t dataSize){

   MassStorageDevice *device = (MassStorageDevice*)d;

   uint32_t dataToWrite = dataSize;
   if(logicalBlockAddress * device->blockSize + dataSize > _dataSize){
      dataToWrite = _dataSize - logicalBlockAddress * device->blockSize;
   }

   memcpy(device->data+ logicalBlockAddress * device->blockSize, data, dataToWrite);

   return dataToWrite;
}

MassStorageDevice massStorageDeviceMock_init(void *data, uint32_t dataSize, uint32_t blockSize){
   MassStorageDevice device = {
      .blockSize = blockSize,
      .data = data,
      .read = read,
      .write = write,
   };

   _dataSize = dataSize;

   return device;
}
void massStorageDeviceMock_free(MassStorageDevice device){}
