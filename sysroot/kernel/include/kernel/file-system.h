#ifndef FILE_SYSTEM_H_INCLUDED
#define FILE_SYSTEM_H_INCLUDED

#include "stdint.h"

typedef struct File{
   void *file;
   void *fileSystem;
   uint32_t (*read)(
         struct File file,
         void *data,
         uint32_t size);
   void (*write)(
         struct File file,
         void *data,
         uint32_t size);
   void(*delete)(
         struct File file);
}File;

typedef struct FileSystem{
   void *data;
   File (*newFile)(
         struct FileSystem filesystem,
         char *path);
   File(*open)(
         struct FileSystem filesystem,
         char *path);
}FileSystem;


#endif
