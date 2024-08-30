#include "kernel/buffered-storage.h"
#include "kernel/memory.h"
#include "stdlib.h"
#include "stdio.h"

uint32_t bufferedStorage_read(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer,
      uint32_t startAddress,
      uint32_t size,
      void *result){

   uint32_t dataToRead = size;
   while(dataToRead > 0){
      uint32_t bufferSize = buffer->count * device->blockSize;
      uint32_t bufferAddress = buffer->blockAddress * device->blockSize;
      if(startAddress >= bufferAddress && startAddress < bufferAddress+ bufferSize){
         uint32_t dataToReadFromBuffer = dataToRead; 
         if(startAddress + dataToRead > bufferAddress + bufferSize){
            dataToReadFromBuffer = bufferAddress + bufferSize - startAddress;
         }
         memcpy(result, buffer->data + startAddress - bufferAddress, dataToReadFromBuffer);
         startAddress += dataToReadFromBuffer;
         result += dataToReadFromBuffer;
         dataToRead -= dataToReadFromBuffer;
      }
      if(dataToRead > 0){
         if(buffer->hasNewData){
            bufferedStorage_writeBuffer(device, buffer);
         }
         uint32_t logicalBlockAddress = startAddress / device->blockSize;
         device->read(device->data, logicalBlockAddress, buffer->data, buffer->maxCount * device->blockSize);
         buffer->blockAddress = logicalBlockAddress;
         buffer->count = buffer->maxCount;
      }
   }
   return size;
}

void bufferedStorage_write(
      MassStorageDevice *device,
      BufferedStorageBuffer *buffer,
      uint32_t startAddress,
      uint32_t size,
      void *result){

   uint32_t dataToWrite = size;
   while(dataToWrite > 0){
      uint32_t bufferSize = buffer->count * device->blockSize;
      uint32_t bufferAddress = buffer->blockAddress * device->blockSize;
      if(startAddress >= bufferAddress && startAddress < bufferAddress + bufferSize){
         uint32_t dataToWriteToBuffer = dataToWrite; 
         if(startAddress + dataToWrite > bufferAddress + bufferSize){
            dataToWriteToBuffer = bufferAddress + bufferSize - startAddress;
         }
         memcpy(buffer->data + startAddress - bufferAddress, result, dataToWriteToBuffer);
         startAddress += dataToWriteToBuffer;
         result += dataToWriteToBuffer;
         dataToWrite -= dataToWriteToBuffer;
         buffer->hasNewData = 1;
      }
      //TODO: If i am writing a full block, there is no point in reading it into the buffer, modifying it and then rewriting it.
      if(dataToWrite > 0){
         if(buffer->hasNewData){
            bufferedStorage_writeBuffer(device, buffer);
         }

         uint32_t logicalBlockAddress = startAddress / device->blockSize;
         device->read(device->data, logicalBlockAddress, buffer->data, buffer->maxCount * device->blockSize);
         buffer->blockAddress = logicalBlockAddress;
         buffer->count = buffer->maxCount;
      }
   }
}


BufferedStorageBuffer* bufferedStorage_newBuffer(uint32_t blockCount, uint32_t blockSize){
   BufferedStorageBuffer *buffer = kmalloc(sizeof(BufferedStorageBuffer));
   *buffer = (BufferedStorageBuffer){
      .data = kmalloc(blockCount * blockSize),
      .blockAddress = 0,
      .count = 0,
      .maxCount = blockCount,
      .hasNewData = 0
   };
   return buffer;
}

void bufferedStorage_freeBuffer(MassStorageDevice *device, BufferedStorageBuffer *buffer){
   bufferedStorage_writeBuffer(device, buffer);
   kfree(buffer->data);
   kfree(buffer);
}

void bufferedStorage_writeBuffer(MassStorageDevice *device, BufferedStorageBuffer *buffer){
   device->write(device->data, buffer->blockAddress, buffer->data, buffer->count * device->blockSize);
}
