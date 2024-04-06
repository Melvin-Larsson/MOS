#ifndef FAT_H_INCLUDED
#define FAT_H_INCLUDED

#include "stdint.h"
#include "mass-storage.h"
#include "file-system.h"

#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ARCHIVE     0x20


typedef enum{
    FatStatusSuccess,
    FatStatusFailure,
    FatStatusInvalidDataCluster,
    FatStatusCouldNotFindEnoughClusters,
    FatInvalidFileName
}FatStatus;

typedef enum{
    Fat12,
    Fat16,
    Fat32,
}FatVersion;

/*
 * Bios Parameters Block (BPB)
 *
 * All numbers are little endian.
 *
 */
typedef struct{
   uint8_t bootJmp[3];
   uint64_t oemIdentifier; //Name identifier. Can be any value.
   uint16_t bytesPerSector; //512, 1024, 2048 or 4096
   uint8_t sectorsPerCluster; //2^n where n > 0
   uint16_t reservedSectorsCount; //Not 0. Used to align data area to multiples of cluster size
   uint8_t fatCount; //2 is recomended
   uint16_t rootDirectoryEntriesCount; //0 for FAT32
   uint16_t sectorsCount; //0 for FAT32.
   uint8_t mediaDescriptorType;
   uint16_t sectorsPerFat;
   uint16_t sectorsPerTrack;
   uint16_t headCount;
   uint32_t hiddenSectorsCount;
   uint32_t largeSectorCount;
}__attribute__((packed)) BiosParameterBlock;

typedef struct{
    uint8_t driveNumber;
    uint8_t reserved;
    uint8_t bootSignature;
    uint32_t volumeSerialNumber;
    uint8_t volumeLabel[11];
    uint8_t fileSystemTypeString[8];
    uint8_t reserved1[448];
    uint32_t signatureWord;
}__attribute__((packed))ExtendedBiosParameterBlockFat12;

typedef struct{
    uint32_t sectorsPerFat;
    uint16_t flags;
    uint16_t fatVersion;
    uint32_t rootCluster;
    uint16_t fsinfoSector;
    uint16_t backupBootSector;
    uint8_t reserved[12];
    uint8_t driveNumber;
    uint8_t windowsFlags;
    uint8_t signature;
    uint32_t volumeId;
    char volumeLabel[11];
    char systemIdentifier[8];
    uint8_t bootCode[420];
    uint16_t bootSignature;   
}__attribute__((packed))ExtendedBootRecordFat32;

typedef struct{
    uint32_t leadSignature;
    uint8_t reserved1[480];
    uint32_t anotherSignature;
    uint32_t freeClusterCount;
    uint32_t clusterHint;
    uint8_t reserved2[12];
    uint32_t trailSignature;
}__attribute__((packed))Fat32FsInfo;

typedef struct{
    BiosParameterBlock parameterBlock;
    ExtendedBootRecordFat32 extendedBootRecordFat32;
    Fat32FsInfo fat32FsInfo;
}__attribute__((packed))DiskInfo;//FIXME: rename

typedef struct{
    MassStorageDevice *device;
    DiskInfo diskInfo;
    FatVersion version;
}FatDisk;

typedef struct{
    uint8_t fileName[11];              // Short file name limited to 11 characters (8.3 format)
    uint8_t attributes;             // File attribute types
    uint8_t reserved;          // Reserved, must be set to 0
    uint8_t creationTimeTenth;      // Component of the file creation time (tenths of a second)
    uint16_t creationTime;          // Creation time (granularity is 2 seconds)
    uint16_t creationDate;          // Creation date
    uint16_t lastAccessDate;        // Last access date
    uint16_t firstClusterHigh;      // High word of first data cluster number (only valid for FAT32)
    uint16_t writeTime;             // Last modification (write) time
    uint16_t writeDate;             // Last modification (write) date
    uint16_t firstClusterLow;       // Low word of first data cluster number
    uint32_t fileSize;              // Size in bytes of file/directory
}__attribute__((packed))FatDirectoryEntry;


typedef struct Cluster{
    uint32_t number;
    uint32_t clusterNumberInFile;
    void *data;
}Cluster;

typedef struct{
    void *data;
    uint32_t nrInFile;
    uint32_t count;
    uint32_t maxCount;
}ClusterBuffer;


typedef struct{
    FatDirectoryEntry directoryEntry;
    uint32_t sector;
    uint32_t entryIndex;
    
    ClusterBuffer buffer;
}FatFile;

FatStatus fat_init(MassStorageDevice* device, FileSystem2 *result);

FatStatus fat_getFileSize(FatDisk *fatDisk, char *filename, uint32_t *result);
FatStatus fat_readFile(FatDisk *fatDisk, char *filename, void *buffer, uint32_t bufferSize);
FatStatus fat_readDirectory(FatDisk *fatDisk, char *directoryname, char *buffer, uint32_t bufferSize);

FatStatus fat_getAvailiableSpace(FatDisk *fatDisk);
FatStatus fat_getDiskSize(FatDisk *fatDisk);



#endif
