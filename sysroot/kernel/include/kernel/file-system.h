#ifndef FILE_SYSTEM_H_INCLUDED
#define FILE_SYSTEM_H_INCLUDED

#include "stdint.h"

struct FileSystem;

typedef struct{
   void *file;
   struct FileSystem *fileSystem;

   char* name;
   uint32_t offset;
}File;

typedef struct{
   void *data;
   struct FileSystem *fileSystem;

   char* name;
   uint32_t offset;
}Directory;

typedef struct{
   char *path;
   char *filename;
}DirectoryEntry;

typedef struct FileSystem{
   void *data;

   void (*closeFileSystem)(struct FileSystem *fileSystem);
   File *(*openFile)(struct FileSystem *fileSystem, char* filename);
   File *(*createFile)(struct FileSystem *fileSytem, char* filename);
   void (*closeFile)(File *file);
   int (*remove)(struct FileSystem *fileSystem, char* file);

   uint32_t (*readFile)(File *file, void *buffer, uint32_t size);
   void (*writeFile)(File *file, void *buffer, uint32_t size);


   Directory *(*openDirectory)(struct FileSystem *fileSystem, char *directoryName);
   Directory *(*createDirectory)(struct FileSystem *fileSystem, char *directoryName);
   void (*closeDirectory)(Directory *dir);
   DirectoryEntry* (*readDirectory)(Directory *dir);

}FileSystem;

void directoryEntry_free(DirectoryEntry *entry);

#endif
