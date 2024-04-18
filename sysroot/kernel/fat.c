#include "kernel/fat.h"
#include "kernel/fat-disk.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "kernel/buffered-storage.h"


#define BLOCK_BUFFER_SIZE 20


void closeFileSystem(struct FileSystem *fileSystem);
static File *openFile(FileSystem *fileSystem, char *path);
static File* createFile(FileSystem *fileSystem, char *path);
void closeFile(File *file);
static int remove(FileSystem *fileSystem, char  *path);
static uint32_t readFile(File *file, void *buffer, uint32_t size);
static void writeFile(File *file, void*buffer, uint32_t size);
static Directory *openDirectory(struct FileSystem *fileSystem, char *directoryName);
static Directory *createDirectory(struct FileSystem *fileSystem, char *directoryName);
static void closeDirectory(Directory *dir);
static DirectoryEntry* readDirectory(Directory *dir);

static int directoryContains(FatDisk *disk, FatFile *dir, char *filename);

static FatFile *findChild(FatDisk *disk, FatFile *file, char *path);

static int isValidFileName(char *name);

static void getFileName(FatDirectoryEntry entry, uint8_t result[13]);
static FatStatus strToFilename(char *str, char dst[11]);

static uint32_t removeTrailingSpaces(uint8_t *arr, uint32_t size, uint8_t *result);

static char* findLast(char *str, char val);
static char* fileNameFromPath(char *path);
static char* parentFromPath(char *path);
static uint32_t equalPrefixLength(const char *s1, const char *s2);

FatStatus fat_init(MassStorageDevice* device, FileSystem *result){
   FatDisk *disk = malloc(sizeof(FatDisk));
   fatDisk_init(device, disk);

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
   bufferedStorage_freeBuffer(disk->device, disk->buffer);
}

static File *openFile(FileSystem *fileSystem, char *path){
   FatDisk *disk = fileSystem->data;

   FatFile *root = fatDisk_openRoot(disk);
   FatFile *file = findChild(disk, root, path);
   fatDisk_closeFile(disk, root);

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
static File* createGenericFile(FileSystem *fileSystem, char *path, uint8_t attributes){
   FatDisk *disk = fileSystem->data;

   char *fileName = fileNameFromPath(path);
   if(!isValidFileName(fileName)){
      free(fileName);
      return 0;
   }
   if(strlen(fileName) > 12){
      free(fileName);
      printf("Invalid filename\n");
      return 0;
   }
   char fatFileName[11];
   strToFilename(fileName, fatFileName);
   
   char *parentPath = parentFromPath(path);
   FatFile *parent;
   if(!*parentPath){
      parent = fatDisk_openRoot(disk);
   }else{
      FatFile *root = fatDisk_openRoot(disk);
      parent = findChild(disk, root, parentPath);
      if(parent == 0){
         printf("no parent\n");
         return 0;
      }
      fatDisk_closeFile(disk, root);
   }
   if(directoryContains(disk, parent, fileName)){
      fatDisk_closeFile(disk, parent);
      free(parentPath);
      free(fileName);
      return 0;

   }
   FatFile *file = fatDisk_newFile(disk, parent, fatFileName, attributes);

   File *result = malloc(sizeof(File));
   *result = (File){
      .file = file,
      .fileSystem = fileSystem, 
      .name = fileName,
      .offset = 0,
   };

   free(parentPath);
   fatDisk_closeFile(disk, parent);

   return result;
}

static File* createFile(FileSystem *system, char *path){
   return createGenericFile(system, path, 0);
}

void closeFile(File *file){
   FatDisk *fatDisk = file->fileSystem->data;
   FatFile *fatFile = file->file;
   fatDisk_closeFile(fatDisk, fatFile);
   free(file->name);
}

static uint32_t readFile(File *file, void *buffer, uint32_t size){
   FatDisk *fatDisk = file->fileSystem->data;
   FatFile *fatFile = file->file;
   uint32_t dataRead = fatDisk_readFile(fatDisk, fatFile, file->offset, size, buffer);
   file->offset += dataRead;
   return dataRead;
}
static void writeFile(File *file, void *buffer, uint32_t size){
   FatDisk *fatDisk = file->fileSystem->data;
   FatFile *fatFile = file->file;
   fatDisk_writeFile(fatDisk, fatFile, file->offset, size, buffer);
   file->offset += size;
}

static Directory *openDirectory(struct FileSystem *fileSystem, char *directoryName){
   if(strlen(directoryName) == 1 && *directoryName == '/'){
      FatDisk *disk = fileSystem->data;
      FatFile *file = fatDisk_openRoot(disk);
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
   uint32_t pathLength = strlen(directoryName);
   char *path = malloc(pathLength + 1);
   strcpy(path, directoryName);
   if(path[pathLength - 1] == '/'){
      path[pathLength - 1] = 0;
   }

   File *file = openFile(fileSystem, path);
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
   free(path);
   return result;
}
static Directory *createDirectory(struct FileSystem *fileSystem, char *directoryName){
   int pathLength = strlen(directoryName);
   char *path = malloc(pathLength + 1);
   strcpy(path, directoryName);
   if(path[pathLength - 1] == '/'){
      path[pathLength - 1] = 0;
   }

   File *file = createGenericFile(fileSystem, path, ATTR_DIRECTORY);
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
static void closeDirectory(Directory *dir){
   FatFile *file = dir->data;
   FatDisk *disk = dir->fileSystem->data;
   fatDisk_closeFile(disk, file);
   free(dir->name);
}

static DirectoryEntry *readDirectory(Directory *directory){
   FatDisk *fatDisk = directory->fileSystem->data;
   FatFile *fatFile = directory->data;
  
   FatDirectoryEntry entry;
   int index = fatDisk_readDirectory(fatDisk, fatFile, directory->offset, &entry);
   directory->offset = index + 1;
   if(index == -1){
      return 0;
   }
   

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
static int directoryContains(FatDisk *disk, FatFile *dir, char *filename){
   FatDirectoryEntry entry;
   uint32_t size = 1;
   for(uint32_t i = 0; size; i++){
      size = fatDisk_readFile(disk, dir, i * sizeof(FatDirectoryEntry), sizeof(FatDirectoryEntry), &entry);
      if(!size){
         return 0;
      }

      char buffer[100];
      getFileName(entry, (uint8_t*)buffer);
      if(equals(buffer, filename)){
         return 1;
      }
   }
   return 0;
}
static int remove(FileSystem *fileSystem, char  *path){
   FatDisk *disk = fileSystem->data;

   FatFile *root = fatDisk_openRoot(disk);
   FatFile *file = findChild(disk, root, path);
   fatDisk_closeFile(disk, root);

   if(!file){
      return 0;
   }
   if(file->isRoot){
      fatDisk_closeFile(disk, file);
      return 0;
   }

   fatDisk_deleteFile(disk, file);
   return 1;
}

static FatFile *findChild(FatDisk *disk, FatFile *file, char *path){
   if(*path == '/'){
      path++;
   }

   FatDirectoryEntry entry; 
   for(int index = 0; index != -1; index++){
      index = fatDisk_readDirectory(disk, file, index, &entry);
      if(index == -1){
         break;
      }
      uint8_t fileName[13] = "/";
      getFileName(entry, fileName);
      uint32_t l = equalPrefixLength((char*)fileName, path);
      if(l == (uint32_t)strlen((char *)fileName)){
         if(equals((char*)fileName, path)){
            return fatDisk_openChild(disk, file, index);
         }
         FatFile *childDirectory = fatDisk_openChild(disk, file, index);
         FatFile *result = findChild(disk, childDirectory, path + strlen((char*)fileName) + 1);
         fatDisk_closeFile(disk, childDirectory);
         return result;
      }
   }
   return 0;
}


int count(char *str, char v){
   int count = 0;
   while(*str){
      if(*str == v){
         count++;
      }
      str++;
   }
   return count;
}
static int isValidFileName(char *name){
   if(strContains(name, "/") ||
      count(name, '.') > 1 ||
      equals(name, ".") ||
      strContains(name, " ") ||
      strlen(name) == 0){
      return 0;
   }
   return 1;
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
   char *cpy = malloc(strlen(str) + 1);
   strcpy(cpy, str);
   toupper(cpy);

   for(int i = 0; i < 8 && *cpy && *cpy != '.'; i++){
      dst[i] = *cpy++;
   }

   if(*cpy == '.'){
      cpy++;
      for(int i = 0; i < 3 && *cpy; i++){
         dst[8 + i] = *cpy++;
      }
      return FatStatusSuccess;
   }else if(*cpy == 0){
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
