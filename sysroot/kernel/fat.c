#include "kernel/fat.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"


#define MIN_DATA_CLUSTER_NUMBER 2
#define BLOCK_BUFFER_SIZE 20


void closeFileSystem(struct FileSystem *fileSystem);
static File *openFile(FileSystem *fileSystem, char *path);
static File* createFile(FileSystem *fileSystem, char *path);
void closeFile(File *file);
static void remove(FileSystem *fileSystem, char  *path);
static uint32_t readFile(File *file, void *buffer, uint32_t size);
static void writeFile(File *file, void*buffer, uint32_t size);
static Directory *openDirectory(struct FileSystem *fileSystem, char *directoryName);
static Directory *createDirectory(struct FileSystem *fileSystem, char *directoryName);
static void closeDirectory(Directory *dir);
static DirectoryEntry* readDirectory(Directory *dir);

static uint32_t getClusterNumberInChain(FatDisk *disk, uint32_t startCluster, uint32_t clusterNumber);
static File fatFileToFile(FatDisk *fatDisk, FatFile *file);
static File *readDirectoryContents(File file, uint32_t *size);
static void delete(struct File file);
static FatStatus strToFilename(char *str, char dst[11]);
static FatFile *openRoot(FatDisk *disk);
static FatFile* directoryEntryToFile(FatDisk *disk, FatDirectoryEntry entry, uint32_t directoryEntryAddress);
static FatFile *findChild(FatDisk *disk, FatFile *file, char *path);
static int isEmpty(FatDirectoryEntry entry);
static uint32_t getBaseAddress(FatDisk *disk, FatFile *file);
static FatFile* addDirectoryEntry(FatDisk *disk, FatFile *parent, FatDirectoryEntry newEntry);
static uint32_t parseDirectoryEntries(void *files, uint32_t size, FatFile *result, uint32_t resultCount, uint32_t startSector, uint32_t sectorSize);
static uint32_t countDirectoryEntries(void *data, uint32_t size);
static uint32_t resizeClusterChain(FatDisk *disk, uint32_t startCluster, uint32_t *result, uint32_t newClusterCount);
static void dealocateClusterChain(FatDisk* disk, uint32_t startCluster);
static uint32_t readClusterNumbers(FatDisk* disk, uint32_t dataCluster, uint32_t *result, uint32_t resultCount);
static uint32_t findFreeClusters(FatDisk* disk, uint32_t result[], uint32_t clusterCount);
static FatStatus writeFatEntry(FatDisk *disk, uint32_t cluster, uint32_t value);
static FatStatus readFatEntry(FatDisk *disk, uint32_t cluster, uint32_t *result);

static uint32_t bl_readFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *result);
static uint32_t bl_readRoot12_16(FatDisk *disk, BlockBuffer *buffer, uint32_t offset, uint32_t size, void *result);
static uint32_t bl_writeFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *data); 
static uint32_t bl_writeRoot12_16(FatDisk *disk, BlockBuffer *buffer, uint32_t offset, uint32_t size, void *data);
static uint32_t bl_readData(FatDisk *disk, BlockBuffer *blockBuffer, uint32_t address, uint32_t size, void *result);
static void bl_storeBuffer(FatDisk *disk, BlockBuffer *blockBuffer);
static void bl_closeFile(FatDisk *disk, FatFile *file);
static void bl_writeData(FatDisk *disk, BlockBuffer *blockBuffer, uint32_t address, uint32_t size, void *data);

static uint32_t getFatOffset(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getFatType(FatDisk *disk);
static uint32_t getCountOfClusters(FatDisk *disk);
static uint32_t getSectorsPerFat(FatDisk *disk);
static uint32_t getSectorsCount(FatDisk *disk);
static uint32_t getRootDirSectorCount(FatDisk *disk);
static uint32_t getFatEntryOffset(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getFatSectorNumber(FatDisk *disk, uint32_t clusterNumber);
static uint32_t getClusterSize(FatDisk *disk);
static uint32_t removeTrailingSpaces(uint8_t *arr, uint32_t size, uint8_t *result);
static void getFileName(FatDirectoryEntry entry, uint8_t result[13]);
static uint32_t getDataAddress(FatDisk *disk);
static uint32_t getClusterAddress(FatDisk *disk, uint32_t cluster);

static char* findLast(char *str, char val);
static char* fileNameFromPath(char *path);
static char* parentFromPath(char *path);
static uint32_t equalPrefixLength(const char *s1, const char *s2);

FatStatus fat_init(MassStorageDevice* device, FileSystem *result){
   FatDisk *disk = malloc(sizeof(FatDisk));
   *disk = (FatDisk){
      .device = device,
      .fatTable = (BlockBuffer){
         .data = malloc(BLOCK_BUFFER_SIZE * device->blockSize),
         .hasNewData = 0,
         .maxCount = BLOCK_BUFFER_SIZE,
         .size = 0,
      },
   };
   device->read(device->data, 0, &disk->diskInfo, sizeof(DiskInfo));

   disk->version = getFatType(disk);

   *result = (FileSystem){
      .data = disk,
      .closeFileSystem = closeFileSystem,
      .openFile = openFile,
      .createFile = createFile,
      .closeFile = closeFile,
      .remove = remove,
      .readFile = readFile,
      .writeFile = writeFile,
      .openDirectory = openDirectory,
      .createDirectory = createDirectory,
      .closeDirectory = closeDirectory,
      .readDirectory = readDirectory,
   };

   return FatStatusSuccess;
}
void closeFileSystem(struct FileSystem *fileSystem){
   FatDisk *disk = fileSystem->data;
   bl_storeBuffer(disk, &disk->fatTable);
   free(disk->fatTable.data);
}

static File *openFile(FileSystem *fileSystem, char *path){
   FatDisk *disk = fileSystem->data;

   FatFile *root = openRoot(disk);
   FatFile *file = findChild(disk, root, path);
   bl_closeFile(disk, root);

   if(!file){
      return 0;
   }
   File *result = malloc(sizeof(File));
   *result = (File){
      .file = file,
      .fileSystem = fileSystem,
      .name = malloc(13),
      .offset = 0,
   };
   getFileName(file->directoryEntry, (uint8_t*)result->name);
   return result;
}
void debug_logMemory();
static File* createFile(FileSystem *fileSystem, char *path){
   FatDisk *disk = fileSystem->data;


   char *fileName = fileNameFromPath(path);
   if(strlen(fileName) > 12){
      free(fileName);
      printf("Invalid filename\n");
      return 0;
   }
   char fatFileName[11];
   strToFilename(fileName, fatFileName);
   
   char *parentPath = parentFromPath(path);
   printf("finding parent |%s|\n", parentPath);
   FatFile *parent;
   if(!*parentPath){
      parent = openRoot(disk);
   }else{
      FatFile *root = openRoot(disk);
      parent = findChild(disk, root, parentPath);
      if(parent == 0){
         printf("no parent\n");
      }
      bl_closeFile(disk, root);
   }
   uint8_t parentName[13];
   getFileName(parent->directoryEntry, parentName);
   printf("Parent :%s\n", parentName);
   for(int i = 0; i < 11; i++){
      printf("%c", parent->directoryEntry.fileName[i]);
   }
   printf("\n");

   uint32_t cluster;
   findFreeClusters(disk, &cluster, 1);
   printf("Using cluster %X\n", cluster);
   writeFatEntry(disk, cluster, 0xFFFFFFFF);

   FatDirectoryEntry entry = (FatDirectoryEntry){
      .firstClusterLow = cluster & 0xFFFF,
      .firstClusterHigh = cluster >> 16,
   };
   memcpy(&entry.fileName, fatFileName, 11);
   FatFile *newFile = addDirectoryEntry(disk, parent, entry);

//    printf("New contents:");
//    debug_logMemory();
//    for(int i = 0; i < 5 * sizeof(FatDirectoryEntry); i++){
//       uint8_t result;
//       bl_readFile(disk, parent, i, 1, &result);
//       printf("%X", result);
//    }
//    printf("\n");

   File *result = malloc(sizeof(File));
   *result = (File){
      .file = newFile,
      .fileSystem = fileSystem, 
      .name = fileName,
      .offset = 0,
   };

   free(parentPath);
   bl_closeFile(disk, parent);

   return result;
}

void closeFile(File *file){
   FatDisk *fatDisk = file->fileSystem->data;
   FatFile *fatFile = file->file;
   bl_closeFile(fatDisk, fatFile);
   free(file->name);
}

static uint32_t readFile(File *file, void *buffer, uint32_t size){
   FatDisk *fatDisk = file->fileSystem->data;
   FatFile *fatFile = file->file;
   uint32_t dataRead = bl_readFile(fatDisk, fatFile, file->offset, size, buffer);
   file->offset += dataRead;
   return dataRead;
}
static void writeFile(File *file, void *buffer, uint32_t size){
   FatDisk *fatDisk = file->fileSystem->data;
   FatFile *fatFile = file->file;
   bl_writeFile(fatDisk, fatFile, file->offset, size, buffer);
   file->offset += size;
}

void debug_logMemory();
static Directory *openDirectory(struct FileSystem *fileSystem, char *directoryName){
   if(strlen(directoryName) == 1 && *directoryName == '/'){
      FatDisk *disk = fileSystem->data;
      FatFile *file = openRoot(disk);
      Directory *result = malloc(sizeof(Directory));

      *result = (Directory){
         .data = file,
         .fileSystem = fileSystem,
         .name = malloc(2),
         .offset = 0,
      };
      strcpy(result->name, "/");
      return result;
   }

   File *file = openFile(fileSystem, directoryName);
   if(!file){
      return 0;
   }
   Directory *result = malloc(sizeof(Directory));

   *result = (Directory){
      .data = file->file,
      .fileSystem = fileSystem,
      .name = file->name,
      .offset = 0,
   };
   free(file);
   return result;
}
static Directory *createDirectory(struct FileSystem *fileSystem, char *directoryName){
   File *file = createFile(fileSystem, directoryName);

   FatFile *fatFile = file->file;
   FatDisk *disk = fileSystem->data;
   fatFile->directoryEntry.attributes |= ATTR_DIRECTORY;

   //FIXME: A bit of a hack
   BlockBuffer buffer = (BlockBuffer){
      .data = malloc(BLOCK_BUFFER_SIZE * disk->device->blockSize),
      .size = 0,
      .hasNewData = 0,
      .maxCount = BLOCK_BUFFER_SIZE,
   };
   bl_writeData(disk, &buffer, fatFile->directoryEntryAddress, sizeof(FatDirectoryEntry), &fatFile->directoryEntry);
   bl_storeBuffer(disk, &buffer);
   free(buffer.data);

   Directory *result = malloc(sizeof(Directory));

   *result = (Directory){
      .data = file->file,
      .fileSystem = fileSystem,
      .name = file->name,
      .offset = 0,
   };
   free(file);
   return result;
}
static void closeDirectory(Directory *dir){
   FatFile *file = dir->data;
   FatDisk *disk = dir->fileSystem->data;
   bl_closeFile(disk, file);
   free(dir->name);
}

static DirectoryEntry *readDirectory(Directory *directory){
   FatDisk *fatDisk = directory->fileSystem->data;
   FatFile *fatFile = directory->data;
   printf("r %X\n", fatFile->directoryEntry.fileSize);
  
   FatDirectoryEntry entry;
   uint32_t dataRead = bl_readFile(fatDisk, fatFile, directory->offset, sizeof(FatDirectoryEntry), &entry);
   printf("data read %d\n", dataRead);
   if(dataRead == 0 ||  isEmpty(entry)){
      return 0;
   }
   directory->offset += sizeof(FatDirectoryEntry);

   DirectoryEntry *result = malloc(sizeof(DirectoryEntry));
   char *filename = malloc(13);
   getFileName(entry, (uint8_t*)filename);
   char *path = malloc(strlen(directory->name) + 1);
   strcpy(path, directory->name);
   *result = (DirectoryEntry){
      .filename = filename,
      .path = path,
   };
   return result;
}
static void remove(FileSystem *fileSystem, char  *path){
   FatDisk *disk = fileSystem->data;

   FatFile *root = openRoot(disk);
   FatFile *file = findChild(disk, root, path);
   bl_closeFile(disk, root);

   if(!file || file->isRoot){
      return;
   }

   uint32_t cluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
   uint32_t directoryEntryAddress = file->directoryEntryAddress;
   bl_closeFile(disk, file);
   dealocateClusterChain(disk, cluster);

   FatDirectoryEntry entry = {};
   entry.fileName[0] = 0xE5;

   //FIXME: A bit of a hack
   BlockBuffer buffer = (BlockBuffer){
      .data = malloc(BLOCK_BUFFER_SIZE * disk->device->blockSize),
      .size = 0,
      .hasNewData = 0,
      .maxCount = BLOCK_BUFFER_SIZE,
   };
   bl_writeData(disk, &buffer, directoryEntryAddress, sizeof(FatDirectoryEntry), &entry);
   bl_storeBuffer(disk, &buffer);
   free(buffer.data);
}


static File fatFileToFile(FatDisk *fatDisk, FatFile *file){
   char* name = malloc(13);
   getFileName(file->directoryEntry, (uint8_t*)name);
   return (File){
      .file = file,
      .fileSystem = fatDisk,
   };
}

static FatFile *openRoot(FatDisk *disk){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;

   FatFile *file = malloc(sizeof(FatFile));
   *file = (FatFile){
      .isRoot = 1,
      .blockBuffer = (BlockBuffer){
         .data = malloc(BLOCK_BUFFER_SIZE * disk->device->blockSize),
         .size = 0,
         .maxCount = BLOCK_BUFFER_SIZE,
         .hasNewData = 0,
      },
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
static FatFile* directoryEntryToFile(FatDisk *disk, FatDirectoryEntry entry, uint32_t directoryEntryAddress){
   FatFile *result = malloc(sizeof(FatFile));
   *result = (FatFile){
      .isRoot = 0,
      .directoryEntry = entry,
      .directoryEntryAddress = directoryEntryAddress,
      .blockBuffer = (BlockBuffer){
         .data = malloc(BLOCK_BUFFER_SIZE * disk->device->blockSize),
         .hasNewData = 0,
         .maxCount = BLOCK_BUFFER_SIZE,
         .size = 0,
      },
   };
   return result;
}
static FatFile *findChild(FatDisk *disk, FatFile *file, char *path){
   FatDirectoryEntry entry; 
//    uint32_t address = getBaseAddress(disk, file);
   uint32_t readSize = 1;

   for(int i = 0; readSize > 0; i++){
      uint8_t fileName[13] = "/";
//       getFileName(file->directoryEntry, fileName);
//       printf("ft %s\n", fileName);
      readSize = bl_readFile(disk, file, i * sizeof(FatDirectoryEntry), sizeof(FatDirectoryEntry), &entry);
      if(isEmpty(entry)){
         continue;
      }
//       printf("ft\n");
      getFileName(entry, fileName);
      uint32_t l = equalPrefixLength((char*)fileName, path);
      if(l == (uint32_t)strlen((char *)fileName)){
         printf("searching %s %s\n", path, fileName);
         //FIXME: Hella uggly
         uint32_t offset = i *  sizeof(FatDirectoryEntry);
         uint32_t address;
         if(file->isRoot && (disk->version == Fat12 || disk ->version == Fat16)){
            BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
            address = (bpb->reservedSectorsCount + bpb->fatCount * bpb->sectorsPerFat) * bpb->bytesPerSector + offset;
         }else{
            uint32_t clusterInFile = offset / getClusterSize(disk);
            uint32_t offsetInFile = offset % getClusterSize(disk);
            uint32_t startCluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
            uint32_t cluster = getClusterNumberInChain(disk, startCluster, clusterInFile);
            address = getClusterAddress(disk, cluster) + offsetInFile;
         }

         if(equals((char*)fileName, path)){
            return directoryEntryToFile(disk, entry, address);
         }
         FatFile *childDirectory = directoryEntryToFile(disk, entry, address);
         FatFile *result = findChild(disk, childDirectory, path + strlen((char*)entry.fileName) + 1);
         bl_closeFile(disk, childDirectory);
         return result;
      }
   }
   return 0;
}

static int isEmpty(FatDirectoryEntry entry){
   return entry.fileName[0] == 0x00 || entry.fileName[0] == 0xE5;
}
static uint32_t getBaseAddress(FatDisk *disk, FatFile *file){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   if((disk->version == Fat12 || disk->version == Fat16) && file->isRoot){
      uint32_t rootSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk);
      return rootSector * bpb->bytesPerSector;
   }

   uint32_t cluster = file->directoryEntry.firstClusterHigh << 16 | file->directoryEntry.firstClusterLow;
   return getClusterAddress(disk, cluster);
}
static FatFile* addDirectoryEntry(FatDisk *disk, FatFile *parent, FatDirectoryEntry newEntry){
   FatDirectoryEntry entry;
   uint32_t dataRead = bl_readFile(disk, parent, 0, sizeof(FatDirectoryEntry), &entry);
   uint32_t address = getBaseAddress(disk, parent);
   uint32_t offset = 0;

   for(int i = 1; !isEmpty(entry) && dataRead > 0; i++){
      dataRead = bl_readFile(disk, parent, i * sizeof(FatDirectoryEntry), sizeof(FatDirectoryEntry), &entry);
      offset += sizeof(FatDirectoryEntry);
   }

   FatFile *file = malloc(sizeof(FatFile));
   *file = (FatFile){
      .isRoot = 0,
      .directoryEntry = newEntry,
      .directoryEntryAddress = address + offset,
   };
   bl_writeFile(disk, parent, offset, sizeof(FatDirectoryEntry), &newEntry);

   file->blockBuffer = (BlockBuffer){
      .data = malloc(BLOCK_BUFFER_SIZE * disk->device->blockSize),
      .size = 0,
      .maxCount = BLOCK_BUFFER_SIZE,
      .hasNewData = 0,
   };

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

   bl_readData(disk, &disk->fatTable, sectorAddress + fatEntryOffset, 4, result);

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
   bl_readData(disk, &disk->fatTable, sectorAddress + fatEntryOffset, 4, &oldValue);

   if(disk->version == Fat12){
      uint16_t newValue;
      if(cluster & 0x1){
         newValue = (oldValue & 0x000F) | (value << 4);
      }else{
         newValue = (oldValue & 0xF000) | (value & 0x0FFF);
      }
      bl_writeData(disk, &disk->fatTable, sectorAddress + fatEntryOffset, 2, &newValue);
   }

   else if(disk->version == Fat16){
      bl_writeData(disk, &disk->fatTable, sectorAddress + fatEntryOffset, 2, &value);
   }

   else if(disk->version == Fat32){
      uint32_t newValue = (oldValue & 0xF0000000) | (value & 0x0FFFFFFF);
      bl_writeData(disk, &disk->fatTable, sectorAddress + fatEntryOffset, 4, &newValue);
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

static uint32_t bl_readFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *result){
   if(file->directoryEntry.fileSize == 0){
      printf("Empty file!");
      return 0;
   }
   if(file->isRoot && (disk->version == Fat12 || disk->version == Fat16)){
      return bl_readRoot12_16(disk, &file->blockBuffer, offset, size, result);
   }

   uint32_t totalData = size;
   if(file->directoryEntry.fileSize < offset + size){
      totalData = file->directoryEntry.fileSize - offset;
      printf("resize %X\n", file->directoryEntry.fileSize);
   }
   printf("file size: %X\n", totalData);

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

      bl_readData(disk, &file->blockBuffer, address, dataFromCluster, result);
      printf("sz %d\n", disk->device->blockSize);
      dataToRead -= dataFromCluster;
      offset += dataFromCluster;
      result += dataFromCluster;
   }
   return totalData;
}
static uint32_t bl_readRoot12_16(FatDisk *disk, BlockBuffer *buffer, uint32_t offset, uint32_t size, void *result){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t rootSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk);
   uint32_t rootAddress = rootSector * bpb->bytesPerSector;

   uint32_t totalData = size;
   if(offset + size > bpb->rootDirectoryEntriesCount * 32){
      totalData = bpb->rootDirectoryEntriesCount * 32 - offset;
   }
   bl_readData(disk, buffer, rootAddress + offset, totalData, result);
   return totalData;
}
static uint32_t bl_writeFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *data){ 
   if(file->isRoot && (disk->version == Fat12 || disk->version == Fat16)){
      return bl_writeRoot12_16(disk, &file->blockBuffer, offset, size, data);
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
      bl_writeData(disk, &file->blockBuffer, address, dataToCluster, data);

      localClusterIndex++;
      offset += dataToCluster;
      dataToWrite -= dataToCluster;
      data += dataToCluster;
   }
   file->directoryEntry.fileSize = offset + totalData;

   //FIXME: A bit of a hack
   BlockBuffer buffer = (BlockBuffer){
      .data = malloc(BLOCK_BUFFER_SIZE * disk->device->blockSize),
      .size = 0,
      .hasNewData = 0,
      .maxCount = BLOCK_BUFFER_SIZE
   };
   bl_writeData(disk, &buffer, file->directoryEntryAddress, sizeof(FatDirectoryEntry), &file->directoryEntry);
   bl_storeBuffer(disk, &buffer);
   free(buffer.data);

   free(clusterNumbers);
   return totalData;
}
static uint32_t bl_writeRoot12_16(FatDisk *disk, BlockBuffer *buffer, uint32_t offset, uint32_t size, void *data){
   BiosParameterBlock *bpb = &disk->diskInfo.parameterBlock;
   uint32_t rootSector = bpb->reservedSectorsCount + bpb->fatCount * getSectorsPerFat(disk);
   uint32_t rootAddress = rootSector * bpb->bytesPerSector;

   uint32_t totalData = size;
   if(offset + size > bpb->rootDirectoryEntriesCount * 32){
      totalData = bpb->rootDirectoryEntriesCount * 32 - offset;
   }
   bl_writeData(disk, buffer, rootAddress + offset, totalData, data);
   return totalData;
}
static uint32_t bl_readData(FatDisk *disk, BlockBuffer *blockBuffer, uint32_t address, uint32_t size, void *result){
   uint32_t dataToRead = size;
   while(dataToRead > 0){
      if(address >= blockBuffer->startAddress && address < blockBuffer->startAddress + blockBuffer->size){
         uint32_t dataToReadFromBuffer = dataToRead; 
         if(address + dataToRead > blockBuffer->startAddress + blockBuffer->size){
            dataToReadFromBuffer = blockBuffer->startAddress + blockBuffer->size - address;
         }
         printf("addr %X (%d)\n", blockBuffer->data + address - blockBuffer->startAddress, disk->device->blockSize);
         printf("res %X\n", result);
         printf("size %d\n", size);
         memcpy(result, blockBuffer->data + address - blockBuffer->startAddress, dataToReadFromBuffer);
         printf("bs3 %d\n", disk->device->blockSize);
         address += dataToReadFromBuffer;
         result += dataToReadFromBuffer;
         dataToRead -= dataToReadFromBuffer;
      }
      if(dataToRead > 0){
         if(blockBuffer->hasNewData){
            bl_storeBuffer(disk, blockBuffer);
         }
         printf("addr %X\n", disk->device);
         printf("bs1 %d\n", disk->device->blockSize);
         uint32_t logicalBlockAddress = address / disk->device->blockSize;
         disk->device->read(disk->device->data, logicalBlockAddress, blockBuffer->data, blockBuffer->maxCount * disk->device->blockSize);
         printf("bs2 %d\n", disk->device->blockSize);
         blockBuffer->startAddress = logicalBlockAddress * disk->device->blockSize;
         blockBuffer->size = blockBuffer->maxCount * disk->device->blockSize;
//          printf("read to buffer %X %X. Asked: %X %X\n", blockBuffer->startAddress, blockBuffer->size, address, dataToRead);
      }
   }
   return size;
}
static void bl_storeBuffer(FatDisk *disk, BlockBuffer *blockBuffer){
   disk->device->write(disk->device->data, blockBuffer->startAddress / disk->device->blockSize, blockBuffer->data, blockBuffer->size);
   blockBuffer->hasNewData = 0;
}
static void bl_closeFile(FatDisk *disk, FatFile *file){
   bl_storeBuffer(disk, &file->blockBuffer);
   free(file->blockBuffer.data);
   free(file);
}
static void bl_writeData(FatDisk *disk, BlockBuffer *blockBuffer, uint32_t address, uint32_t size, void *data){
   uint32_t dataToWrite = size;
   while(dataToWrite > 0){
      if(address >= blockBuffer->startAddress && address < blockBuffer->startAddress + blockBuffer->size){
         uint32_t dataToWriteToBuffer = dataToWrite; 
         if(address + dataToWrite > blockBuffer->startAddress + blockBuffer->size){
            dataToWriteToBuffer = blockBuffer->startAddress + blockBuffer->size - address;
         }
         memcpy(blockBuffer->data + address - blockBuffer->startAddress, data, dataToWriteToBuffer);
         address += dataToWriteToBuffer;
         data += dataToWriteToBuffer;
         dataToWrite -= dataToWriteToBuffer;
         blockBuffer->hasNewData = 1;
      }
      //TODO: If i am writing a full block, there is no point in reading it into the buffer, modifying it and then rewriting it.
      if(dataToWrite > 0){
         if(blockBuffer->hasNewData){
            bl_storeBuffer(disk, blockBuffer);
         }

         uint32_t logicalBlockAddress = address / disk->device->blockSize;
         disk->device->read(disk->device->data, logicalBlockAddress, blockBuffer->data, blockBuffer->maxCount * disk->device->blockSize);
         blockBuffer->startAddress = logicalBlockAddress * disk->device->blockSize;
         blockBuffer->size = blockBuffer->maxCount * disk->device->blockSize;
      }
   }
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
static void getFileName(const FatDirectoryEntry entry, uint8_t result[13]){
   uint8_t name[8],  extension[3];
   memcpy(name, entry.fileName, 8);
   memcpy(extension, &entry.fileName[8], 3);

   uint32_t nameLength = removeTrailingSpaces(name, 8, result);
   result[nameLength] = '.';
   uint32_t extensionLength = removeTrailingSpaces(extension, 3, result + nameLength + 1);
   if(extensionLength == 0){
      result[nameLength] = 0;
   }else{
      result[nameLength + extensionLength + 1] = 0;
   }
   result[12] = 0;
   tolower((char*)result);
}

static FatStatus strToFilename(char *str, char dst[11]){
   memset(dst, ' ', 11);
   toupper(str);

   for(int i = 0; i < 8 && *str && *str != '.'; i++){
      dst[i] = *str++;
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

static uint32_t removeTrailingSpaces(uint8_t *arr, uint32_t size, uint8_t *result){
   int end = size - 1;
   while(arr[end] == ' ' && end >= 0){
      end--;
   }
   for(int i = 0; i <= end; i++){
      result[i] = arr[i];
   }
   return end + 1;
}
static char* findLast(char *str, char val){
   char *result = 0;
   while(*str){
      if(*str == val){
         result = str;
      }
      str++;
   }
   return result;
}

static char* fileNameFromPath(char *path){
   char *last = findLast(path, '/');
   if(!last){
      char* result = malloc(strlen(path) + 1);
      strcpy(result, path);
      return result;
   }
   uint32_t length = strlen(path) - (last - path) - 1;
   char *result = malloc(length + 1);
   memcpy(result, last + 1, length + 1);
   return result;
}
static char* parentFromPath(char *path){
   char *last = findLast(path, '/');
   if(!last){
      char *result = malloc(1);
      *result = 0;
      return result;
   }
   uint32_t length = last - path;
   char *result = malloc(length + 1);
   memcpy(result, path, length);
   result[length] = 0;
   return result;
}
static uint32_t equalPrefixLength(const char *s1, const char *s2){
   uint32_t count = 0;
   while(*s1 && *s2 && *s1 == *s2){
      s1++;
      s2++;
      count++;
   }   
   return count;
}
