#include "kernel/memory.h"
#include "testrunner.h"
#include "stdio.h"
#include "buffered-storage.c"

#define DATA_SIZE 1024 * 1024

#define UNUSED(x) (void)(x)

static uint32_t blockSize;
static char *data;
static BufferedStorageBuffer *buffer;
static MassStorageDevice device;

int readFake(void *device, uint32_t logicalBlockAddress, void *result, uint32_t bufferSize){
   UNUSED(device);

   uint32_t address = logicalBlockAddress * blockSize; 
   memcpy(result, data + address, bufferSize);
   return bufferSize;
}
void f(){

}
int writeFake(void *device, uint32_t logicalBlockAddress, void *dataToWrite, uint32_t dataSize){
   UNUSED(device);

   uint32_t address = logicalBlockAddress * blockSize; 
   memcpy(data + address, dataToWrite, dataSize);

   
   return dataSize;
}


TEST_GROUP_SETUP(group){
    data = kmalloc(DATA_SIZE);
    memset(data, 0x69, DATA_SIZE);
    blockSize = 512;
    device = (MassStorageDevice){
        .blockSize = blockSize,
        .read = readFake,
        .write = writeFake
    };
    buffer = bufferedStorage_newBuffer(5, blockSize);
}
TEST_GROUP_TEARDOWN(group){
   kfree(data);
}

TESTS

TEST(group, read){
    char readData[8];
    for(int i = 0; i < DATA_SIZE / 8; i++){
       memset(readData, 0, sizeof(readData));
       bufferedStorage_read(&device, buffer, i * 8, 8, readData);
       if(!assertArray(readData, 8, &data[i*8], 8)){
          break;
       }
    }
    bufferedStorage_freeBuffer(&device, buffer);
}

TEST(group, writeBigData){
    memset(data, 0, DATA_SIZE);
    char writeData[1024];
    memset(writeData, 0x69, sizeof(writeData));
    for(int i = 0; i < 1024; i++){
      bufferedStorage_write(&device, buffer, i * 1024, 1024, writeData);
    }

    bufferedStorage_freeBuffer(&device, buffer);
    for(int i = 0; i < (DATA_SIZE) / 8; i++){
       if(!assertArray(&data[i * 8], 8, writeData, 8)){
          break;
       }
    }
}
TEST(group, writeMultipleBuffers){
   char writeData[DATA_SIZE / 2];
   memset(writeData, 1, DATA_SIZE / 2);
   bufferedStorage_write(&device, buffer, 0, DATA_SIZE / 2, writeData);
   bufferedStorage_write(&device, buffer, DATA_SIZE/2, DATA_SIZE / 2, writeData);
   bufferedStorage_freeBuffer(&device, buffer);

   for(int i = 0; i < DATA_SIZE / 8; i++){
      if(!assertArray(&data[i * 8], 8, writeData, 8)){
         printf("i: %d\n", i);
         break;
      }
   }
}

TEST(group, write_smallData){
    for(int i = 0; i < 512; i += 3){
      uint8_t toWrite[] = {i, i + 1, i + 2};
      bufferedStorage_write(&device, buffer, i, 3, toWrite);
    }

    bufferedStorage_freeBuffer(&device, buffer);

    for(int i = 0; i < 512; i += 3){
      char expected[] = {i, i + 1, i + 2};
      if(!assertArray(&data[i], 3, expected, 3)){
         break;
      }
    }
}
TEST(group, write_twoThirdsOfMemory_restUnchanged){
   for(int i = 0; i < 512; i += 3){
     uint8_t toWrite[] = {i, i + 1};
     bufferedStorage_write(&device, buffer, i, 2, toWrite);
   }

   bufferedStorage_freeBuffer(&device, buffer);

   for(int i = 0; i < 512; i += 3){
     char expected[] = {i, i + 1};
     if(!assertArray(&data[i], 2, expected, 2)){
        break;
     }
     uint8_t *val = (uint8_t*)&data[i+2];
     assertInt(*val, 0x69);
   }  
}

TEST(group, write_blockEdge){
   char dataToWrite[69];
   memset(dataToWrite, 0x42, sizeof(dataToWrite));
   bufferedStorage_write(&device, buffer, 500, 69, dataToWrite);
   bufferedStorage_freeBuffer(&device, buffer);

   char expected[512];
   memset(expected, 0x69, sizeof(expected));

   assertArray(data, 500, expected, 500);
   assertArray(&data[500], 69, dataToWrite, 69);
   assertArray(&data[500 + 69], 1024 - 500 - 69, expected, 1024 - 500 - 69);
}


END_TESTS
