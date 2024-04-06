#include "kernel/fat.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"


#define MIN_DATA_CLUSTER_NUMBER 2


static uint8_t* readFile(FatDisk *disk, FatFile *file, uint32_t maxClusterCount, uint32_t *clusterCount);
static FatStatus writeFile(FatDisk *disk, FatFile *file, void *data, uint32_t size);
static FatFile* readRootDirectory(FatDisk *disk, uint32_t *resultCount);
static FatFile* readRootDirectoryFat32(FatDisk *disk, uint32_t *resultCount);
static FatFile* readRootDirectoryFat12_16(FatDisk *disk, uint32_t *resultCount);
static FatFile* readDirectory(FatDisk *disk, uint32_t cluster, uint32_t *resultCount);
static void writeDirectoryEntry(FatDisk *disk, FatFile *file, FatDirectoryEntry newEntry);
static FatFile* addDirectoryEntry(FatDisk *disk, FatFile *parent, FatDirectoryEntry newEntry);
static uint32_t parseDirectoryEntries(void *files, uint32_t size, FatFile *result, uint32_t resultCount, uint32_t startSector, uint32_t sectorSize);
static uint32_t countDirectoryEntries(void *data, uint32_t size);
static uint8_t* readClusterChain(FatDisk *disk, uint32_t dataCluster, uint32_t maxClusterCount, uint32_t *clusterCountResult);
static uint32_t readClusterNumbers(FatDisk* disk, uint32_t dataCluster, uint32_t *result, uint32_t resultCount);
static uint32_t resizeClusterChain(FatDisk *disk, uint32_t startCluster, uint32_t *result, uint32_t newClusterCount);
static uint32_t findFreeClusters(FatDisk* disk, uint32_t result[], uint32_t clusterCount);
static void dealocateClusterChain(FatDisk* disk, uint32_t startCluster);
static FatStatus readFatEntry(FatDisk *disk, uint32_t cluster, uint32_t *result);
static FatStatus writeFatEntry(FatDisk *disk, uint32_t cluster, uint32_t value);
static FatStatus readDataClusters(FatDisk *disk, uint32_t cluster, uint32_t count, uint8_t *result);
static FatStatus writeDataClusters(FatDisk *disk, uint32_t cluster, void *data, uint32_t clusterCount);
static FatStatus readSectors(FatDisk *disk, uint8_t *result, uint32_t sectorNumber, uint32_t count);
static FatStatus writeSectors(FatDisk *disk, uint8_t *data, uint32_t sectorNumber, uint32_t count);
static uint32_t getFatOffset(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getFatType(FatDisk *disk);
static uint32_t getCountOfClusters(FatDisk *disk);
static uint32_t getSectorsPerFat(FatDisk *disk);
static uint32_t getSectorsCount(FatDisk *disk);
static uint32_t getRootDirSectorCount(FatDisk *disk);
static uint32_t getFatEntryOffset(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getFatSectorNumber(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getClusterSize(FatDisk *disk);

void getFileName(const FatFile *file, uint8_t result[13]);

static File newFile(FileSystem fileSystem, char *path);
static File open(FileSystem filesystem, char *path);

uint32_t read(struct File file,void *data,uint32_t size);
void write(struct File file,void *data,uint32_t size);
void delete(struct File file);

FatStatus fat_init(MassStorageDevice* device, FileSystem *result){
   FatDisk *disk = malloc(sizeof(FatDisk));
   *disk = (FatDisk){
      .device = device,
   };
   device->read(device->data, 0, &disk->diskInfo, sizeof(DiskInfo));

   disk->version = getFatType(disk);

   *result = (FileSystem){
      .data = disk,
      .newFile = newFile,
      .open = open,
   };
   return FatStatusSuccess;
}

uint32_t read(struct File file, void *data, uint32_t size){
   FatDisk *fatDisk = file.fileSystem;
   FatFile *fatFile = file.file;
   BiosParameterBlock *bpb = &fatDisk->diskInfo.parameterBlock;
   uint32_t bytesPerCluster = bpb->bytesPerSector * bpb->sectorsPerCluster;
   uint32_t maxClusters = (size + bytesPerCluster + 1) / bytesPerCluster;

   uint32_t clusterCount;
   uint8_t *result = readFile(fatDisk, fatFile, maxClusters, &clusterCount);

   uint32_t dataRead = size;
   if(size > fatFile->directoryEntry.fileSize){
      dataRead = fatFile->directoryEntry.fileSize;
   }
   memcpy(data, result, dataRead);
   free(result);
   return dataRead;
}
void write(struct File file, void *data, uint32_t size){
   FatDisk *fatDisk = file.fileSystem;
   FatFile *fatFile = file.file;
   writeFile(fatDisk, fatFile, data, size);
}
void delete(struct File file){
   FatDisk *fatDisk = file.fileSystem;
   FatFile *fatFile = file.file;

   uint32_t cluster = fatFile->directoryEntry.firstClusterHigh << 16 | fatFile->directoryEntry.firstClusterLow;
   dealocateClusterChain(fatDisk, cluster);

   FatDirectoryEntry newEntry;
   memset(&newEntry, 0, sizeof(FatDirectoryEntry));
   writeDirectoryEntry(fatDisk, fatFile, newEntry);

   free(file.file);
}

static void toUpper(char *str){
   while(*str){
      if(*str > 'a' && *str <= 'z'){
         *str += 'A' - 'a';
      }
      str++;
   }
}
static FatStatus strToFilename(char *str, char dst[11]){
   memset(dst, ' ', 11);
   toUpper(str);

   for(int i = 0; i < 8 && *str && *str != '.'; i++){
      dst[i] = *str++;
      printf("%c-", dst[i]);
   }

   if(*str == '.'){
      str++;
      for(int i = 0; i < 3 && *str; i++){
         dst[8 + i] = *str++;
      }
      return FatStatusSuccess;
   }else if(*str == 0){
      return FatStatusSuccess;
   }
   return FatInvalidFileName;
}

static File newFile(FileSystem fileSystem, char *path){
   uint32_t cluster;
   FatDisk *fatDisk = fileSystem.data;
   printf("here: %X\n", fatDisk->version);
   BiosParameterBlock *bpb = &fatDisk->diskInfo.parameterBlock;

   findFreeClusters(fatDisk, &cluster, 1);
   printf("Cluster %X\n", cluster);
   writeFatEntry(fatDisk, cluster, 0xFFFFFFFF);
   uint32_t val;
   readFatEntry(fatDisk, cluster, &val);
   printf("val %X\n", val);

   FatDirectoryEntry entry = (FatDirectoryEntry){
      .firstClusterLow = cluster & 0xFFFF,
      .firstClusterHigh = cluster >> 16,
   };
   char filename[11];
   if(strToFilename(path, filename) != FatStatusSuccess){
      printf("Invalid filename %s\n", path);
      return (File){0};
   }
   memcpy(&entry.fileName, filename, 11);

   FatDirectoryEntry parent;
   if(fatDisk->version == Fat32){
      uint32_t parentCluster = fatDisk->diskInfo.extendedBootRecordFat32.rootCluster;
      parent = (FatDirectoryEntry){
         .firstClusterLow = parentCluster & 0xFFFF,
         .firstClusterHigh = parentCluster >> 16
      };
   }else{
      memset(&parent, 0, sizeof(FatDirectoryEntry));
   }
   FatFile parentFile = (FatFile){
      .directoryEntry = parent,
   };
   FatFile *file = addDirectoryEntry(fatDisk, &parentFile, entry);
   return (File){
      .file = file,
      .fileSystem = fatDisk,
      .read = read,
      .write = write,
      .delete = delete
   };
}

static uint32_t equals(char *s1, char *s2){
   while(*s1 && *s2){
      if(*s1 != *s2){
         return 0;
      }
      s1++;
      s2++;
   }
   return *s1 == *s2;
}
static File open(FileSystem filesystem, char *path){
   printf("\n:%s \n", path);
   FatDisk *disk = (FatDisk *)filesystem.data;
   uint32_t resultCount; 
   FatFile *files = readRootDirectory(disk, &resultCount);
   
   for(uint32_t i = 0; i < resultCount; i++){
      char name[13];
      getFileName(&files[i], (uint8_t*)name);
      printf("%s != %s\n", name, path);

      if(equals(name, path)){
         FatFile *fatFile = malloc(sizeof(FatFile));
         memcpy(fatFile, &files[i], sizeof(FatFile));
         free(files);
         return (File){
            .file = fatFile,
            .fileSystem = disk,
            .read = read,
            .write = write,
            .delete = delete,
         };
      }
      printf("no found\n");
   }
   return (File){0,0,0,0, 0}; //FIXME: create new file
}

static uint8_t* readFile(FatDisk *disk, FatFile *file, uint32_t maxClusterCount, uint32_t *resultSize){
   uint32_t cluster = file->directoryEntry.firstClusterLow | (file->directoryEntry.firstClusterHigh << 16);
   uint32_t clusterCount;
   uint8_t *contents = readClusterChain(disk, cluster, maxClusterCount, &clusterCount);
   printf("read %X clusters\n", clusterCount);
   
   if(clusterCount * getClusterSize(disk) < file->directoryEntry.fileSize){
      *resultSize = clusterCount * getClusterSize(disk);
   }else{
      *resultSize = file->directoryEntry.fileSize;
   }
   return contents;
}
static FatStatus writeFile(FatDisk *disk, FatFile *file, void *data, uint32_t size){
   uint32_t clusterCount = (size + getClusterSize(disk) - 1) / getClusterSize(disk);
   uint32_t cluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
   uint32_t *clusterNumbers = malloc(clusterCount * sizeof(uint32_t));
   uint32_t newClusterCount = resizeClusterChain(disk, cluster, clusterNumbers, clusterCount);

   for(uint32_t i = 0; i < newClusterCount; i++){
      FatStatus status = writeDataClusters(disk, clusterNumbers[i], data, 1);
      if(status != FatStatusSuccess){
         free(clusterNumbers);
         return status;
      }
      data += getClusterSize(disk);
   }

   free(clusterNumbers);

   FatDirectoryEntry modifiedEntry = file->directoryEntry;
   modifiedEntry.fileSize = size;
   writeDirectoryEntry(disk, file, modifiedEntry);

   if(newClusterCount < clusterCount){
      return FatStatusCouldNotFindEnoughClusters;
   }
   return FatStatusSuccess;
}

static FatFile* readRootDirectory(FatDisk *disk, uint32_t *resultCount){
   if(disk->version == Fat32){
       return readRootDirectoryFat32(disk, resultCount);
   }
   return readRootDirectoryFat12_16(disk, resultCount);
}
static FatFile* readRootDirectoryFat32(FatDisk *disk, uint32_t *resultCount){
   uint32_t cluster = disk->diskInfo.extendedBootRecordFat32.rootCluster;
   return readDirectory(disk, cluster, resultCount);
}
static FatFile* readRootDirectoryFat12_16(FatDisk *disk, uint32_t *resultCount){
      BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
      uint32_t sectorNumber = bpb->reservedSectorsCount + (bpb->fatCount * bpb->sectorsPerFat);
      uint32_t sizeOfSectors = getRootDirSectorCount(disk) * bpb->bytesPerSector;

      uint8_t *sectorBuffer = malloc(sizeOfSectors);
      readSectors(disk, (uint8_t*)sectorBuffer, sectorNumber, getRootDirSectorCount(disk));

      uint32_t fileCount = countDirectoryEntries(sectorBuffer, sizeOfSectors);
      void *result = malloc(fileCount * sizeof(FatFile));
      parseDirectoryEntries(sectorBuffer, sizeOfSectors, result, fileCount, sectorNumber, sizeOfSectors); 

      free(sectorBuffer);

      *resultCount = fileCount;
      return result;
}

static FatFile* readDirectory(FatDisk *disk, uint32_t cluster, uint32_t *resultCount){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t startSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk) + cluster * bpb->sectorsPerCluster;
   uint32_t sectorSize = bpb->bytesPerSector;

   uint32_t clusterCount;
   uint8_t *clusters = readClusterChain(disk, cluster, (uint32_t)-1, &clusterCount);

   uint32_t sizeOfClusters = clusterCount * getClusterSize(disk);
   uint32_t fileCount = countDirectoryEntries(clusters, sizeOfClusters);

   FatFile *result = malloc(fileCount * sizeof(FatFile));
   parseDirectoryEntries(clusters, sizeOfClusters, result, fileCount, startSector, sectorSize);

   free(clusters);

   *resultCount = fileCount;
   return result;
}
static void writeDirectoryEntry(FatDisk *disk, FatFile *file, FatDirectoryEntry newEntry){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   FatDirectoryEntry *entries = malloc(bpb->bytesPerSector);
   readSectors(disk, (uint8_t*)entries, file->sector, 1);
   entries[file->entryIndex] = newEntry;
   writeSectors(disk, (uint8_t*)entries, file->sector, 1);
}
static FatFile* findInRootSector(FatDisk *disk, int (*p)(FatDirectoryEntry)){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t startSector = bpb->reservedSectorsCount + getSectorsPerFat(disk)  * bpb->fatCount;
   uint32_t sectorCount = getRootDirSectorCount(disk);

   FatDirectoryEntry *buffer = malloc(bpb->bytesPerSector * sectorCount);
   readSectors(disk, (uint8_t*)buffer, startSector, sectorCount);
   for(uint32_t i = 0; i < (bpb->bytesPerSector * sectorCount)/ sizeof(FatDirectoryEntry); i++){
      if(p(buffer[i])){
         FatFile *result = malloc(sizeof(FatFile));
         *result = (FatFile){
            .directoryEntry = buffer[i],
            .sector = startSector + (i * sizeof(FatDirectoryEntry)) / bpb->bytesPerSector,
            .entryIndex = i % (bpb->bytesPerSector / sizeof(FatDirectoryEntry))
         };
         free(buffer);
         return result;
      }
   }
   return 0;
}
static FatFile* findInCluster(FatDisk *disk, FatFile *parent, int(*p)(FatDirectoryEntry)){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t bytesPerCluster = bpb->sectorsPerCluster * bpb->bytesPerSector;
   uint32_t entryCount = bytesPerCluster / sizeof(FatDirectoryEntry);
   uint32_t entriesPerSector = entryCount / bpb->sectorsPerCluster;

   FatDirectoryEntry *buffer = malloc(bytesPerCluster);
   uint32_t cluster = parent->directoryEntry.firstClusterHigh << 16 | parent->directoryEntry.firstClusterLow;
   while(cluster >= MIN_DATA_CLUSTER_NUMBER && cluster <= getCountOfClusters(disk) + 1){
      readDataClusters(disk, cluster, 1, (uint8_t*)buffer);
      for(uint32_t i = 0; i < entryCount; i++){
         if(p(buffer[i])){
            FatFile *result = malloc(sizeof(FatFile));
            *result = (FatFile){
               .directoryEntry = buffer[i],
               .sector = cluster * bpb->sectorsPerCluster + i / entriesPerSector,
               .entryIndex = i % entriesPerSector
            };
            free(buffer);
            return result;
         }
      }
      readFatEntry(disk, cluster, &cluster);
   }
   free(buffer);
   return 0;
}
static int isEmpty(FatDirectoryEntry entry){
   uint32_t *ptr = (uint32_t*)&entry;
   for(uint32_t i = 0; i < sizeof(FatDirectoryEntry) / 4; i++){
      if(*ptr++){
         return 0;
      }
   }
   return 1;
}
static FatFile* addDirectoryEntry(FatDisk *disk, FatFile *parent, FatDirectoryEntry newEntry){
   FatFile *file;
   //FIXME: hack
   if(parent->directoryEntry.firstClusterLow == 0 && parent->directoryEntry.firstClusterHigh == 0){
      if(disk->version != Fat32){
         file = findInRootSector(disk, isEmpty); 
      }else{
         printf("Invalid parent\n");
         return 0;
      }
   }else{
      file = findInCluster(disk, parent, isEmpty);
   }
   printf("file %X %X\n", file->sector, file->entryIndex);
   printf("s %X\n", disk->diskInfo.parameterBlock.bytesPerSector);
   file->directoryEntry = newEntry;
   writeDirectoryEntry(disk, file, newEntry);
   return file;
}
static uint32_t parseDirectoryEntries(void *files, uint32_t size, FatFile *result, uint32_t resultCount, uint32_t startSector, uint32_t sectorSize){
      FatDirectoryEntry *buffer = (FatDirectoryEntry*)files;
      uint32_t bufferCount = size / sizeof(FatDirectoryEntry);

      uint32_t count = 0;
      for(uint32_t i = 0; i < bufferCount && count < resultCount; i++){
         if(buffer[i].fileName[0] == 0x00){
            break;
         }
         if(buffer[i].fileName[0] == 0xE5){
            continue;
         }
         memcpy(&(result[count].directoryEntry), &buffer[i], sizeof(FatDirectoryEntry));
         result[count].sector = startSector + i / (sectorSize / 32);
         result[count].entryIndex = i % (sectorSize / 32);
         count++;
      }
      return count;
}
static uint32_t countDirectoryEntries(void *data, uint32_t size){
   uint32_t maxFileCount = size / sizeof(FatDirectoryEntry);
   uint32_t count = 0;
   FatDirectoryEntry *files = (FatDirectoryEntry*)data;
   for(uint32_t i = 0; i < maxFileCount; i++){
      if(files[i].fileName[0] == 0x00){
         break;
      }
      if(files[i].fileName[0] == 0xE5){
         continue;
      }
      count++;
   }
   return count;
}

static uint8_t* readClusterChain(FatDisk *disk, uint32_t dataCluster, uint32_t maxClusterCount, uint32_t *clusterCountResult){
   uint32_t *clusters = malloc(maxClusterCount * sizeof(uint32_t));
   uint32_t clusterCount = readClusterNumbers(disk, dataCluster, clusters, maxClusterCount);
   void* buffer = malloc(clusterCount * getClusterSize(disk));

   for(uint32_t i = 0; i < clusterCount; i++){
      int sequentialLength = 1;
      while(clusters[i + sequentialLength] == clusters[i] + sequentialLength){
         sequentialLength++;
      }
      readDataClusters(disk, clusters[i], sequentialLength, buffer);
      i += sequentialLength - 1;
   }
   free(clusters);
   *clusterCountResult = clusterCount;
   return buffer;
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
   writeFatEntry(disk, startCluster, 0);
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
   printf("c: %X\n", clusterCount);
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
   uint32_t sectorNumber = getFatSectorNumber(disk, cluster);
   uint32_t fatEntryOffset = getFatEntryOffset(disk, cluster);
   uint32_t sectorCount = disk->version == Fat12 ? 2 : 1;

   uint8_t buffer[4096 * 2];
   FatStatus readStatus = readSectors(disk, buffer, sectorNumber, sectorCount);
   if(readStatus != FatStatusSuccess){
      return readStatus;
   }
   memcpy(result, &buffer[fatEntryOffset], 4);

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
   uint32_t sectorNumber = getFatSectorNumber(disk, cluster);
   uint32_t fatEntryOffset = getFatEntryOffset(disk, cluster);
   uint32_t sectorCount = disk->version == Fat12 ? 2 : 1;

   uint8_t buffer[4096 * 2];
   FatStatus readStatus = readSectors(disk, buffer, sectorNumber, sectorCount);
   if(readStatus != FatStatusSuccess){
      return readStatus;
   }
   uint32_t oldValue;
   memcpy(&oldValue, &buffer[fatEntryOffset], 4);

   if(disk->version == Fat12){
      uint16_t newValue;
      if(cluster & 0x1){
         newValue = (oldValue & 0x000F) | (value << 4);
      }else{
         newValue = (oldValue & 0xF000) | (value & 0x0FFF);
      }
      memcpy(&buffer[fatEntryOffset], &newValue, 2);
   }

   else if(disk->version == Fat16){
      memcpy(&buffer[fatEntryOffset], &value, 2);
   }

   else if(disk->version == Fat32){
      uint32_t newValue = (oldValue & 0xF0000000) | (value & 0x0FFFFFFF);
      memcpy(&buffer[fatEntryOffset], &newValue, 4);
   }

   return writeSectors(disk, buffer, sectorNumber, sectorCount);
}


static FatStatus readDataClusters(FatDisk *disk, uint32_t cluster, uint32_t count, uint8_t *result){
   if(cluster < MIN_DATA_CLUSTER_NUMBER){
      return FatStatusInvalidDataCluster;
   }
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t firstDataSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk) + getRootDirSectorCount(disk);
   uint32_t sectorNumber = firstDataSector + (cluster - 2) * bpb->sectorsPerCluster;
   uint32_t sectorCount = bpb->sectorsPerCluster * count;
   return readSectors(disk, result, sectorNumber, sectorCount);
}
static FatStatus writeDataClusters(FatDisk *disk, uint32_t cluster, void *data, uint32_t clusterCount){
   if(cluster < MIN_DATA_CLUSTER_NUMBER){
      return FatStatusInvalidDataCluster;
   }
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t firstDataSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk) + getRootDirSectorCount(disk);
   uint32_t sectorNumber = firstDataSector + (cluster - 2) * bpb->sectorsPerCluster;
   uint32_t sectorCount = bpb->sectorsPerCluster * clusterCount;
   return writeSectors(disk, data, sectorNumber, sectorCount);
}

static FatStatus readSectors(FatDisk *disk, uint8_t *result, uint32_t sectorNumber, uint32_t count){
   uint32_t sectorSize = disk->diskInfo.parameterBlock.bytesPerSector;
   uint32_t logicalBlockAddress = (sectorNumber * sectorSize) / disk->device->blockSize;
   uint32_t offset = (sectorNumber * sectorSize) % disk->device->blockSize;

   uint8_t *buffer = malloc(offset + count * sectorSize);

   if(disk->device->read(disk->device->data, logicalBlockAddress, buffer, offset + count * sectorSize) != 0){
      free(buffer);
      return FatStatusFailure;
   }
   memcpy(result, &buffer[offset], sectorSize * count);
   free(buffer);
   return FatStatusSuccess;
}
static FatStatus writeSectors(FatDisk *disk, uint8_t *data, uint32_t sectorNumber, uint32_t count){
   uint32_t sectorSize = disk->diskInfo.parameterBlock.bytesPerSector;
   uint32_t blockSize = disk->device->blockSize;
   uint32_t blockCount = (sectorSize * count + blockSize - 1) / blockSize;
   uint32_t logicalBlockAddress = (sectorNumber * sectorSize) / blockSize;
   uint32_t blockOffset = (sectorNumber * sectorSize) % blockSize;

   uint8_t *buffer = malloc(blockCount * blockSize);
   disk->device->read(disk->device->data, logicalBlockAddress, buffer, blockCount * blockSize);
   memcpy(buffer + blockOffset, data, sectorSize * count);

   if(disk->device->write(disk->device->data, logicalBlockAddress, buffer, blockCount * blockSize) != 0){
      free(buffer);
      return FatStatusFailure;
   }
   free(buffer);
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


uint32_t removeTrailingSpaces(uint8_t *arr, uint32_t size, uint8_t *result){
   uint8_t end = size - 1;
   while(arr[end] == ' ' && end >= 0){
      end--;
   }
   for(int i = 0; i <= end; i++){
      result[i] = arr[i];
   }
   return end + 1;
}
void toLower(uint8_t *str){
   while(*str){
      if(*str <= 'Z' && *str >= 'A'){
         *str -= ('A' - 'a');
      }
      str++;
   }
}
void getFileName(const FatFile *file, uint8_t result[13]){
   uint8_t name[8],  extension[3];
   memcpy(name, &file->directoryEntry.fileName[0], 8);
   memcpy(extension, &file->directoryEntry.fileName[8], 3);

   uint32_t length = removeTrailingSpaces(name, 8, result);
   result[length++] = '.';
   length += removeTrailingSpaces(extension, 3, result + length);
   result[length] = 0;
   toLower(result);
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

static void bufferClusters(FatDisk *disk, ClusterBuffer *buffer, uint32_t startCluster, uint32_t clusterNumberInFile){
      buffer->nrInFile = clusterNumberInFile;
      uint32_t *clusterNumbers = malloc(buffer->maxCount * sizeof(uint32_t));
      uint32_t clusterCount = readClusterNumbers(disk, startCluster, clusterNumbers, buffer->maxCount);

      uint32_t clusterAddress = getClusterAddress(disk, startCluster);
      uint32_t logicalBlockAddress = clusterAddress / disk->device->blockSize;
      uint32_t blockCount = (buffer->maxCount * getClusterSize(disk) + disk->device->blockSize - 1) / disk->device->blockSize;
      void *blockBuffer = malloc(blockCount * disk->device->blockSize);
      disk->device->read(disk->device->data, logicalBlockAddress, blockBuffer, blockCount * disk->device->blockSize);

      uint32_t blockStart = logicalBlockAddress * disk->device->blockSize;
      uint32_t blockEnd = blockStart + blockCount * disk->device->blockSize;
      buffer->count = 0;
      for(uint32_t i = 0; i < clusterCount; i++){
         uint32_t clusterBytes = getClusterAddress(disk, clusterNumbers[i]);
         if(clusterBytes >= blockStart && clusterBytes + getClusterSize(disk) <= blockEnd){
            memcpy(buffer->data + buffer->count * getClusterSize(disk), blockBuffer + clusterBytes - blockStart, getClusterSize(disk));
            buffer->count++;
         }else{
            break;
         }
      }
      free(blockBuffer);
      free(clusterNumbers);
}
static void writeBufferCluster(FatDisk *disk, FatFile *file, uint32_t startCluster){
   ClusterBuffer *buffer = &file->buffer; 

   uint32_t clusterAddress = getClusterAddress(disk, startCluster);
   uint32_t logicalBlockAddress = clusterAddress / disk->device->blockSize;

   uint32_t dataToWrite = buffer->count * getClusterSize(disk);
   void *ptr = buffer->data;
   if(disk->device->blockSize < getClusterSize(disk)){
      while(disk->device->blockSize < dataToWrite){
         disk->device->write(disk->device->data, logicalBlockAddress, ptr, disk->device->blockSize) ;
         logicalBlockAddress++;
         ptr += disk->device->blockSize;
         dataToWrite -= disk->device->blockSize;
      }  
   }
   if(dataToWrite > 0){
      printf("[FAT] writeToBuffer error!");
      while(1);
   }
}

static uint32_t b_readData(FatDisk *disk, FatFile *file, uint32_t start, uint32_t size, void *result){
   uint32_t startClusterInFile = start / getClusterSize(disk);

   uint32_t fileStartCluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
   uint32_t startCluster = getClusterNumberInChain(disk, fileStartCluster, startClusterInFile);

   uint32_t offsetIntoCluster = start % getClusterSize(disk);

   ClusterBuffer *buffer = &file->buffer;
   uint32_t dataToCopy = size;
   if(file->directoryEntry.fileSize < size + start){
      dataToCopy = file->directoryEntry.fileSize - start;
   }
   uint32_t dataRead = dataToCopy;
   while(dataToCopy > 0){
      if(startClusterInFile >= buffer->nrInFile && startClusterInFile < buffer->nrInFile + buffer->count){ 
         uint32_t offsetIntoBuffer = (startClusterInFile - buffer->nrInFile) * getClusterSize(disk) + offsetIntoCluster;
         uint32_t dataToCopyFromBuffer = dataToCopy;
         if(buffer->count * getClusterSize(disk) < size + offsetIntoBuffer){
            dataToCopy = buffer->count * getClusterSize(disk) - offsetIntoBuffer;
            startClusterInFile = buffer->nrInFile + buffer->count;
         }
         memcpy(result, buffer->data + offsetIntoBuffer, dataToCopyFromBuffer);
         result += dataToCopyFromBuffer;
         dataToCopy -= dataToCopyFromBuffer;
         offsetIntoCluster = 0;
      }
      if(dataToCopy > 0){
         bufferClusters(disk, &file->buffer, startCluster, startClusterInFile);
      }
   }
   return dataRead;
}
static uint32_t b_writeData(FatDisk *disk, FatFile *file, uint32_t start, void *data, uint32_t size){
   

}






