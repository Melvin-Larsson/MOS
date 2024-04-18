#ifndef FAT_DISK_H_INCLUDED
#define FAT_DISK_H_INCLUDED

#include "stdint.h"
#include "fat.h"

FatStatus fatDisk_init(MassStorageDevice *device, FatDisk *result);

FatFile *fatDisk_openRoot(FatDisk *disk);

FatFile *fatDisk_openChild(FatDisk *disk, FatFile *parent, int childIndex);

FatFile* fatDisk_newFile(FatDisk *disk, FatFile *parent, char filename[11], uint8_t attributes);
FatFile *fatDisk_newDirectory(FatDisk *disk, FatFile *parent, char filename[11]);

void fatDisk_closeFile(FatDisk *disk, FatFile *file);
uint32_t fatDisk_readFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *result);
uint32_t fatDisk_writeFile(FatDisk *disk, FatFile *file, uint32_t offset, uint32_t size, void *data); 
FatStatus fatDisk_deleteFile(FatDisk *disk, FatFile* file);

int fatDisk_readDirectory(FatDisk *disk, FatFile *directory, uint32_t index, FatDirectoryEntry *result);

#endif
