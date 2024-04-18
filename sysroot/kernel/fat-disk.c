#include "kernel/fat-disk.h"
#include "stdio.h"
#include "stdlib.h"

static uint32_t readClusterNumbers(FatDisk* disk, uint32_t dataCluster, uint32_t *result, uint32_t resultCount);
static uint32_t findFreeClusters(FatDisk* disk, uint32_t result[], uint32_t clusterCount);
static uint32_t resizeClusterChain(FatDisk *disk, uint32_t startCluster, uint32_t *result, uint32_t newClusterCount);
static void dealocateClusterChain(FatDisk* disk, uint32_t startCluster);
static uint32_t getClusterNumberInChain(FatDisk *disk, uint32_t startCluster, uint32_t clusterNumber);
static int isEmpty(FatDirectoryEntry entry);
static int isEndOfDirectory(FatDirectoryEntry entry);
static FatFile* addDirectoryEntry(FatDisk *disk, FatFile *parent, FatDirectoryEntry newEntry);

static uint32_t readRoot12_16(FatDisk *disk, BufferedStorageBuffer *buffer, uint32_t offset, uint32_t size, void *result);
static uint32_t writeRoot12_16(FatDisk *disk, BufferedStorageBuffer *buffer, uint32_t offset, uint32_t size, void *data);

static uint32_t getAddressFromOffset(FatDisk *disk, FatFile *file, uint32_t offset);
static uint32_t getFatOffset(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getFatType(FatDisk *disk);
static uint32_t getCountOfClusters(FatDisk *disk);
static uint32_t getSectorsPerFat(FatDisk *disk);
static uint32_t getSectorsCount(FatDisk *disk);
static uint32_t getRootDirSectorCount(FatDisk *disk);
static uint32_t getFatEntryOffset(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getFatSectorNumber(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getClusterSize(FatDisk *disk);
static uint32_t getDataAddress(FatDisk *disk);
static uint32_t getClusterAddress(FatDisk *disk, uint32_t cluster);
static uint32_t getCluster(FatFile *file);


static FatStatus writeFatEntry(FatDisk *disk, uint32_t cluster, uint32_t value);
static FatStatus readFatEntry(FatDisk *disk, uint32_t cluster, uint32_t *result);


#define MIN_DATA_CLUSTER_NUMBER 2
#define BLOCK_BUFFER_SIZE 20

FatStatus fatDisk_init(MassStorageDevice *device, FatDisk *result){
   *result = (FatDisk){
      .device = device,
      .buffer = bufferedStorage_newBuffer(BLOCK_BUFFER_SIZE, device->blockSize),
   };
   device->read(device->data, 0, &result->diskInfo, sizeof(DiskInfo));

   result->version = getFatType(result);
   return FatStatusSuccess;
}

FatFile *fatDisk_openRoot(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;

   FatFile *file = malloc(sizeof(FatFile));
   *file = (FatFile){
      .isRoot = 1,
      .buffer = bufferedStorage_newBuffer(BLOCK_BUFFER_SIZE, disk->device->blockSize),
      .directoryEntry = (FatDirectoryEntry){
         .fileName = {'/', 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20},
         .attributes = ATTR_DIRECTORY,
      },
   };

   if(disk->version == Fat12 || disk->version == Fat16){
      file->directoryEntry.fileSize = getRootDirSectorCount(disk) * bpb->bytesPerSector;
   }
   else{
      uint32_t rootCluster = disk->diskInfo.extendedBootRecordFat32.rootCluster;
      file->directoryEntry.firstClusterLow = rootCluster & 0xFFFF;
      file->directoryEntry.firstClusterHigh = rootCluster >> 16;
//       file->directoryEntry.fileSize = 0; FIXME
   }

   return file;
}

uint32_t fatDisk_readFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *result){
   if(file->directoryEntry.fileSize == 0){
      return 0;
   }
   if(file->isRoot && (disk->version == Fat12 || disk->version == Fat16)){
      return readRoot12_16(disk, file->buffer, offset, size, result);
   }

   uint32_t totalData = size;
   if(file->directoryEntry.fileSize < offset + size){
      totalData = file->directoryEntry.fileSize - offset;
   }

   uint32_t dataToRead = totalData;
   uint32_t startCluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
   while(dataToRead > 0){
      uint32_t clusterNumberInFile = offset / getClusterSize(disk);   
      uint32_t offsetInCluster = offset % getClusterSize(disk);
      uint32_t clusterNumber = getClusterNumberInChain(disk, startCluster, clusterNumberInFile);
      uint32_t address = getClusterAddress(disk, clusterNumber) + offsetInCluster;
      uint32_t dataFromCluster = dataToRead;
      if(dataToRead > getClusterSize(disk) - offsetInCluster){
         dataFromCluster = getClusterSize(disk) - offsetInCluster;
      }

      bufferedStorage_read(disk->device, file->buffer, address, dataFromCluster, result);
      dataToRead -= dataFromCluster;
      offset += dataFromCluster;
      result += dataFromCluster;
   }
   return totalData;
}
int fatDisk_readDirectory(FatDisk *disk, FatFile *directory, uint32_t startIndex, FatDirectoryEntry *result){
   int index = startIndex - 1;
   do{
      index++;
      uint32_t dataRead = fatDisk_readFile(disk, directory, index * sizeof(FatDirectoryEntry), sizeof(FatDirectoryEntry), result);
      if(dataRead == 0 || isEndOfDirectory(*result)){
         return -1;
      }
   }while(isEmpty(*result));

   return index;
}

uint32_t fatDisk_writeFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *data){ 
   if(file->isRoot && (disk->version == Fat12 || disk->version == Fat16)){
      return writeRoot12_16(disk, file->buffer, offset, size, data);
   }
   uint32_t cluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow; 
   uint32_t clusterCount = (offset + size + getClusterSize(disk) - 1) / getClusterSize(disk);
   uint32_t *clusterNumbers = malloc(clusterCount * sizeof(uint32_t));
   uint32_t newClusterCount = resizeClusterChain(disk, cluster, clusterNumbers, clusterCount);

   uint32_t totalData = size;
   if(newClusterCount * getClusterSize(disk) < offset + size){
      totalData = getClusterSize(disk) * newClusterCount - offset;
   }

   uint32_t dataToWrite = totalData;
   uint32_t localClusterIndex = offset / getClusterSize(disk);
   while(dataToWrite > 0){
      uint32_t currentCluster = clusterNumbers[localClusterIndex]; 
      uint32_t offsetIntoCluster = offset % getClusterSize(disk);
      uint32_t address = getClusterAddress(disk, currentCluster) + offsetIntoCluster;
      uint32_t dataToCluster = dataToWrite;
      if(offsetIntoCluster + dataToWrite > getClusterSize(disk)){
         dataToCluster = getClusterSize(disk) - offsetIntoCluster;
      }
      bufferedStorage_write(disk->device, file->buffer, address, dataToCluster, data);

      localClusterIndex++;
      offset += dataToCluster;
      dataToWrite -= dataToCluster;
      data += dataToCluster;
   }
   if(!file->isRoot){
      file->directoryEntry.fileSize = offset + totalData;

      //FIXME: A bit of a hack
      BufferedStorageBuffer *buffer = bufferedStorage_newBuffer(BLOCK_BUFFER_SIZE, disk->device->blockSize);
      bufferedStorage_write(disk->device, buffer, file->directoryEntryAddress, sizeof(FatDirectoryEntry), &file->directoryEntry);
      bufferedStorage_freeBuffer(disk->device, buffer);
   }

   free(clusterNumbers);
   return totalData;
}
void fatDisk_closeFile(FatDisk *disk, FatFile *file){
   bufferedStorage_freeBuffer(disk->device, file->buffer);
   free(file);
}
FatFile* fatDisk_newFile(FatDisk *disk, FatFile *parent, char filename[11], uint8_t attributes){
   uint32_t cluster;
   findFreeClusters(disk, &cluster, 1);
   writeFatEntry(disk, cluster, 0xFFFFFFFF);

   FatDirectoryEntry entry = (FatDirectoryEntry){
      .firstClusterLow = cluster & 0xFFFF,
      .firstClusterHigh = cluster >> 16,
      .attributes = attributes,
   };
   memcpy(&entry.fileName, filename, 11);
   FatFile *newFile = addDirectoryEntry(disk, parent, entry);

   return newFile;
}
FatStatus fatDisk_deleteFile(FatDisk *disk, FatFile* file){
   uint32_t cluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
   fatDisk_closeFile(disk, file);
   dealocateClusterChain(disk, cluster);

   FatDirectoryEntry entry = {};
   entry.fileName[0] = 0xE5;

   //FIXME: A bit of a hack
   BufferedStorageBuffer *buffer = bufferedStorage_newBuffer(BLOCK_BUFFER_SIZE, disk->device->blockSize);
   bufferedStorage_write(disk->device, buffer, file->directoryEntryAddress, sizeof(FatDirectoryEntry), &entry);
   bufferedStorage_freeBuffer(disk->device, buffer);

   return FatStatusSuccess;
}
FatFile *fatDisk_openChild(FatDisk *disk, FatFile *parent, int childIndex){
   FatDirectoryEntry entry;
   fatDisk_readDirectory(disk, parent, childIndex, &entry);
   if(isEmpty(entry)){
      printf("Entry is empty\n");
      return 0;
   }

   uint32_t offset = childIndex * sizeof(FatDirectoryEntry);
   uint32_t address = getAddressFromOffset(disk, parent, offset); 


   FatFile *result = malloc(sizeof(FatFile));
   *result = (FatFile){
      .directoryEntry = entry,
      .directoryEntryAddress = address,
      .isRoot = 0,
      .buffer = bufferedStorage_newBuffer(BLOCK_BUFFER_SIZE, disk->device->blockSize),
   };
   return result;
}


static uint32_t readClusterNumbers(FatDisk* disk, uint32_t dataCluster, uint32_t *result, uint32_t resultCount){
   uint32_t maxClusterNumber = getCountOfClusters(disk) + 1;

   uint32_t count = 0; 
   while(dataCluster <= maxClusterNumber && dataCluster >= MIN_DATA_CLUSTER_NUMBER && count < resultCount){
      *result++ = dataCluster; 
      count++;
      readFatEntry(disk, dataCluster, &dataCluster);
   }
   return count;
}
static uint32_t findFreeClusters(FatDisk* disk, uint32_t result[], uint32_t clusterCount){
   uint32_t resultIndex = 0;
   for(uint32_t i = 2; i <= getCountOfClusters(disk) + 1 && resultIndex < clusterCount; i++){
      uint32_t entry;
      FatStatus status = readFatEntry(disk, i, &entry);
      if(status != FatStatusSuccess){
         printf("[fat] bad!");
         return 0;
      }
      if(entry == 0){
         result[resultIndex++] = i;
      }
   }
   return resultIndex;
}

static FatStatus readFatEntry(FatDisk *disk, uint32_t cluster, uint32_t *result){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t sectorAddress = getFatSectorNumber(disk, cluster) * bpb->bytesPerSector;
   uint32_t fatEntryOffset = getFatEntryOffset(disk, cluster);

   bufferedStorage_read(disk->device, disk->buffer, sectorAddress + fatEntryOffset, 4, result);

   if(disk->version == Fat12){
      *result &= 0xFFFF;
      if(cluster & 0x1){
         *result >>= 4;
      }else{
         *result &= 0x0FFF;
      }
   }

   else if(disk->version == Fat16){
      *result &= 0xFFFF;
   }

   else if(disk->version == Fat32){
      *result &= 0x0FFFFFFF;
   }
   return FatStatusSuccess;

}
static FatStatus writeFatEntry(FatDisk *disk, uint32_t cluster, uint32_t value){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t sectorAddress = getFatSectorNumber(disk, cluster) * bpb->bytesPerSector;
   uint32_t fatEntryOffset = getFatEntryOffset(disk, cluster);

   uint32_t oldValue;
   bufferedStorage_read(disk->device, disk->buffer, sectorAddress + fatEntryOffset, 4, &oldValue);

   if(disk->version == Fat12){
      uint16_t newValue;
      if(cluster & 0x1){
         newValue = (oldValue & 0x000F) | (value << 4);
      }else{
         newValue = (oldValue & 0xF000) | (value & 0x0FFF);
      }
      bufferedStorage_write(disk->device, disk->buffer, sectorAddress + fatEntryOffset, 2, &newValue);
   }

   else if(disk->version == Fat16){
      bufferedStorage_write(disk->device, disk->buffer, sectorAddress + fatEntryOffset, 2, &value);
   }

   else if(disk->version == Fat32){
      uint32_t newValue = (oldValue & 0xF0000000) | (value & 0x0FFFFFFF);
      bufferedStorage_write(disk->device, disk->buffer, sectorAddress + fatEntryOffset, 4, &newValue);
   }

   return FatStatusSuccess;
}

static uint32_t getFatOffset(FatDisk *disk, uint32_t clusterNumber){
   FatVersion fatVersion = getFatType(disk);
   if(fatVersion == Fat16){
      return clusterNumber * 2;
   }
   if(fatVersion == Fat32){
      return clusterNumber * 4;
   }
   return clusterNumber + (clusterNumber / 2);
}
static uint32_t getFatType(FatDisk *disk){
   uint32_t countOfClusters = getCountOfClusters(disk);
   if(countOfClusters < 4085){
      return Fat12;
   }
   if(countOfClusters < 65525){
      return Fat16;
   }
   return Fat32;
}

static uint32_t getCountOfClusters(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t sectorsPerFat = getSectorsPerFat(disk);
   uint32_t sectorCount = getSectorsCount(disk);
   uint32_t rootDirSectorCount = getRootDirSectorCount(disk);
   uint32_t dataSectorCount = sectorCount - (bpb->reservedSectorsCount + bpb->fatCount * sectorsPerFat + rootDirSectorCount);
   return dataSectorCount / bpb->sectorsPerCluster;
}
static uint32_t getSectorsPerFat(FatDisk *disk){
   DiskInfo *diskInfo = &disk->diskInfo;
   BiosParameterBlock *bpb = &diskInfo->parameterBlock;
   ExtendedBootRecordFat32 *fat32 = &diskInfo->extendedBootRecordFat32;
   if(bpb->sectorsPerFat != 0){
      return bpb->sectorsPerFat;
   }
   return fat32->sectorsPerFat;
}
static uint32_t getSectorsCount(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   if(bpb->sectorsCount != 0){
      return bpb->sectorsCount;
   }
   return bpb->largeSectorCount;
}
static uint32_t getRootDirSectorCount(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   return ((bpb->rootDirectoryEntriesCount * 32) + (bpb->bytesPerSector - 1)) / bpb->bytesPerSector; //Rounding up
}
static uint32_t getFatEntryOffset(FatDisk *disk, uint32_t clusterNumber){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t fatOffset = getFatOffset(disk, clusterNumber);
   return fatOffset % bpb->bytesPerSector;
}
static uint32_t getFatSectorNumber(FatDisk *disk, uint32_t clusterNumber){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t fatOffset = getFatOffset(disk, clusterNumber);
   return bpb->reservedSectorsCount + fatOffset / bpb->bytesPerSector;
}
static uint32_t getClusterSize(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   return bpb->bytesPerSector * bpb->sectorsPerCluster;
}
static uint32_t getDataAddress(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   return (bpb->reservedSectorsCount +
          bpb->fatCount * getSectorsPerFat(disk) +
          getRootDirSectorCount(disk))
       * bpb->bytesPerSector;
}
static uint32_t getClusterAddress(FatDisk *disk, uint32_t cluster){
   return getDataAddress(disk) + (cluster - 2) * getClusterSize(disk);
}

uint32_t readRoot12_16(FatDisk *disk, BufferedStorageBuffer *buffer, uint32_t offset, uint32_t size, void *result){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t rootSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk);
   uint32_t rootAddress = rootSector * bpb->bytesPerSector;

   uint32_t totalData = size;
   if(offset + size > bpb->rootDirectoryEntriesCount * 32){
      totalData = bpb->rootDirectoryEntriesCount * 32 - offset;
   }
   bufferedStorage_read(disk->device, buffer, rootAddress + offset, totalData, result);
   return totalData;
}
uint32_t writeRoot12_16(FatDisk *disk, BufferedStorageBuffer *buffer, uint32_t offset, uint32_t size, void *data){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t rootSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk);
   uint32_t rootAddress = rootSector * bpb->bytesPerSector;

   uint32_t totalData = size;
   if(offset + size > bpb->rootDirectoryEntriesCount * 32){
      totalData = bpb->rootDirectoryEntriesCount * 32 - offset;
   }
   bufferedStorage_write(disk->device, buffer, rootAddress + offset, totalData, data);
   return totalData;
}

static uint32_t getClusterNumberInChain(FatDisk *disk, uint32_t startCluster, uint32_t clusterNumber){
   uint32_t cluster = startCluster;
   while(clusterNumber-- > 0){
      if(cluster < MIN_DATA_CLUSTER_NUMBER || cluster > getCountOfClusters(disk) + 1){
         return 0;
      }
      readFatEntry(disk, cluster, &cluster);
   }
   return cluster;
}

static int isEmpty(FatDirectoryEntry entry){
   return entry.fileName[0] == 0x00 || entry.fileName[0] == 0xE5;
}
static int isEndOfDirectory(FatDirectoryEntry entry){
   return entry.fileName[0] == 0x00;
}
static uint32_t getAddressFromOffset(FatDisk *disk, FatFile *file, uint32_t offset){
   if(file->isRoot && (disk->version == Fat12 || disk->version == Fat16)){
      BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
      uint32_t rootAddress =  (bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk)) * bpb->bytesPerSector;
      return rootAddress + offset;
   }
   uint32_t startCluster = getCluster(file);
   uint32_t clusterOffset = offset / getClusterSize(disk);
   uint32_t clusterNumber = getClusterNumberInChain(disk, startCluster, clusterOffset);
   return getClusterAddress(disk, clusterNumber) + offset % getClusterSize(disk);
}
static FatFile* addDirectoryEntry(FatDisk *disk, FatFile *parent, FatDirectoryEntry newEntry){
   FatDirectoryEntry entry;
   int i = -1;
   do{
      i++;
      uint32_t dataRead = fatDisk_readFile(disk, parent, i * sizeof(FatDirectoryEntry), sizeof(FatDirectoryEntry), &entry);
      if(dataRead == 0){
         break;
      }

   }while(!isEmpty(entry));
   
   uint32_t offset = i * sizeof(FatDirectoryEntry);

   FatFile *file = malloc(sizeof(FatFile));
   *file = (FatFile){
      .isRoot = 0,
      .directoryEntry = newEntry,
      .directoryEntryAddress = getAddressFromOffset(disk, parent, offset),
      .buffer = bufferedStorage_newBuffer(BLOCK_BUFFER_SIZE, disk->device->blockSize),
   };
   fatDisk_writeFile(disk, parent, offset, sizeof(FatDirectoryEntry), &newEntry);
   return file;
}
static uint32_t resizeClusterChain(FatDisk *disk, uint32_t startCluster, uint32_t *result, uint32_t newClusterCount){
   uint32_t currentClusterCount = readClusterNumbers(disk, startCluster, result, newClusterCount);
   if(currentClusterCount < newClusterCount){
      uint32_t foundClusterCount = findFreeClusters(disk, &result[currentClusterCount], newClusterCount - currentClusterCount);
      for(uint32_t i = 0; i < foundClusterCount; i++){
         writeFatEntry(disk, result[currentClusterCount - 1 + i], result[currentClusterCount + i]);
      }
      writeFatEntry(disk, result[currentClusterCount + foundClusterCount - 1], 0xFFFFFFFF);
      return currentClusterCount + foundClusterCount;
   }

   uint32_t nextCluster;
   readFatEntry(disk, result[currentClusterCount - 1], &nextCluster);
   if(nextCluster >= MIN_DATA_CLUSTER_NUMBER && nextCluster <= getCountOfClusters(disk) + 1){
      dealocateClusterChain(disk, nextCluster);
   }
   writeFatEntry(disk, result[currentClusterCount - 1], 0xFFFFFFFF);
   return newClusterCount;
}
static void dealocateClusterChain(FatDisk* disk, uint32_t startCluster){
   while(startCluster >= MIN_DATA_CLUSTER_NUMBER && startCluster <= getCountOfClusters(disk) + 1){
      uint32_t nextCluster;
      FatStatus status = readFatEntry(disk, startCluster, &nextCluster);
      if(status != FatStatusSuccess){
         printf("[fat] deal bad\n");
         return;
      }
      writeFatEntry(disk, startCluster, 0);
      startCluster = nextCluster;
   }
}
static uint32_t getCluster(FatFile *file){
   return file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
}
