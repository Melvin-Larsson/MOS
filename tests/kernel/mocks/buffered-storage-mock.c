#include "buffered-storage-mock.h"
#include "kernel/buffered-storage.h"
#include "kernel/memory.h"
#include "stdlib.h"
#include "stdio.h"


static uint8_t *_data;

void bufferedStorageMock_init(void *data){
   _data = data;
}

#define ENABLED

#ifdef ENABLED

uint32_t bufferedStorage_read(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer,
      uint32_t startAddress,
      uint32_t size,
      void *result){

    memcpy(result, _data + startAddress, size);
    return size;
}

void bufferedStorage_write(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer,
      uint32_t startAddress,
      uint32_t size,
      void *result){

    memcpy(_data + startAddress, result, size);
}

BufferedStorageBuffer* bufferedStorage_newBuffer(
      uint32_t blockCount,
      uint32_t blockSize){

    return kmalloc(sizeof(BufferedStorageBuffer));
}

void bufferedStorage_freeBuffer(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer){

    kfree(buffer);
}

void bufferedStorage_writeBuffer(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer
      ){

    return;
}
#else
#include "buffered-storage.c"
#endif
