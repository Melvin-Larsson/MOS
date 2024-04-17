#ifndef BUFFERED_STORAGE_H_INCLUDED
#define BUFFERED_STORAGE_H_INCLUDED

#include "kernel/mass-storage.h"

typedef struct{
    void *data;
    uint32_t blockAddress;
    uint32_t count;
    uint32_t maxCount;
    uint8_t hasNewData;
}BufferedStorageBuffer;

uint32_t bufferedStorage_read(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer,
      uint32_t startAddress,
      uint32_t size,
      void *result);

void bufferedStorage_write(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer,
      uint32_t startAddress,
      uint32_t size,
      void *result);

BufferedStorageBuffer* bufferedStorage_newBuffer(
      uint32_t blockCount,
      uint32_t blockSize);

void bufferedStorage_freeBuffer(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer);

void bufferedStorage_writeBuffer(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer
      );




#endif
