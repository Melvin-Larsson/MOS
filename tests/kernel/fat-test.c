#include "fat.c"
#include "fat-disk.c"
#include "file-system.c"
#include "testrunner.h"
#include "empty-disk.h"
#include "mass-storage-device-mock.h"
#include "buffered-storage-mock.h"

#include "string.h"

#define MEMORY_SIZE 1024 * 1024


static uint8_t *data;
static FileSystem fatSystem;
static MassStorageDevice *device;



int createTreeHelper(char *parent, int depth){
  if(depth == 0){
    return 1;
  }
  for(int i = 0; i < 3; i++){
    char path[100];
    sprintf(path, "%sc%d", parent, i);
    Directory *directory = fatSystem.createDirectory(&fatSystem, path);
    int status = assertIntNotEquals((uint32_t)directory, 0);
    fatSystem.closeDirectory(directory);
    if(!status){
      return 0;
    }
    sprintf(path, "%sc%d/", parent, i);

    if(!createTreeHelper(path, depth - 1)){
      return 0;
    }
  }
  return 1;
}
int verifyTree(char *parent, int depth){
  if(depth == 0){
    return 1;
  } 


  Directory *parentDir = fatSystem.openDirectory(&fatSystem, parent);
  uint32_t found = 0;
  for(int i = 0; i < 3; i++){
    DirectoryEntry *entry = fatSystem.readDirectory(parentDir);
    int status = assertIntNotEquals((uint32_t)entry, 0);
    status |= assertInt(entry->filename[0], 'c');
    uint32_t curr = entry->filename[1] - '0';
    found |= 1 << curr;
    directoryEntry_free(entry);
    if(!status){
      fatSystem.closeDirectory(parentDir);
      return 0;
    }
  }
  if(!assertInt(found, 0b111)){
    fatSystem.closeDirectory(parentDir);
    return 0;
  }

  for(int i = 0; i < 3; i++){
    char path[100];
    sprintf(path, "%sc%d/", parent, i);
    if(!verifyTree(path, depth - 1)){
      fatSystem.closeDirectory(parentDir);
      return 0;
    }
  }
  fatSystem.closeDirectory(parentDir);
  return 1;
}

TEST_GROUP_SETUP(string){}
TEST_GROUP_TEARDOWN(string){}

TEST_GROUP_SETUP(read_write){
  data = kmalloc(MEMORY_SIZE);
  bufferedStorageMock_init(data);
  memset(data, 0, MEMORY_SIZE);
  if(sizeof(bytes) > MEMORY_SIZE){
    printf("data > memory\n");
    while(1);
  }
  memcpy(data, bytes, sizeof(bytes));

  device = massStorageDeviceMock_init(data, MEMORY_SIZE, 512);

  fat_init(device, &fatSystem);
}
TEST_GROUP_TEARDOWN(read_write){
  kfree(data); 
  massStorageDeviceMock_free(device);
  fatSystem.closeFileSystem(&fatSystem);
}

TESTS

TEST(read_write, createDirectory_okName_resultCorrectName){
  Directory *directory = fatSystem.createDirectory(&fatSystem, "dir");
  assertString(directory->name, "dir");
  fatSystem.closeDirectory(directory);
}

TEST(read_write, openDirectory_inRootDirectory_matchingDirectoryName){
  Directory *directory = fatSystem.createDirectory(&fatSystem, "test");
  fatSystem.closeDirectory(directory);


  Directory *root = fatSystem.openDirectory(&fatSystem, "/");
  DirectoryEntry *entry = fatSystem.readDirectory(root);
  assertString(entry->filename, "test");

  fatSystem.closeDirectory(root);
  directoryEntry_free(entry);
}

TEST(read_write, createDirectory_trailingAndLeadingSlash_successfull){
  Directory *dir = fatSystem.createDirectory(&fatSystem, "/dir/");
  fatSystem.closeDirectory(dir);

  dir = fatSystem.openDirectory(&fatSystem, "dir");
  assertString(dir->name, "dir");
  fatSystem.closeDirectory(dir);
}

TEST(read_write, createDirectory_reservedNames_nullPointers){
  char *reservedDirectoryNames[] = {"", " ", "  ", ".", "..", "...", "/", "//", "///"};
  for(uint32_t i = 0; i < sizeof(reservedDirectoryNames) / sizeof(uint8_t*); i++){
    Directory *directory = fatSystem.createDirectory(&fatSystem, reservedDirectoryNames[i]);
    if(!assertInt((uintptr_t)directory, 0)){
      fatSystem.closeDirectory(directory);
      break;
    }       
  } 
}
TEST(read_write, createDirectory_createGrandChilds){
  Directory *child = fatSystem.createDirectory(&fatSystem, "child");  
  assertIntNotEquals((uintptr_t)child, 0);
  fatSystem.closeDirectory(child);
  Directory *grandchild = fatSystem.createDirectory(&fatSystem, "child/gc");  
  assertIntNotEquals((uintptr_t)grandchild, 0);
  fatSystem.closeDirectory(grandchild);

  Directory *root = fatSystem.openDirectory(&fatSystem, "/");
  DirectoryEntry *childEntry = fatSystem.readDirectory(root);

  assertIntNotEquals((uintptr_t)childEntry, 0);
  assertString(childEntry->filename, "child");
  childEntry = fatSystem.readDirectory(root);
  assertInt((uint32_t)childEntry, 0);
  fatSystem.closeDirectory(root);
//   directoryEntry_free(childEntry); //FIXME: Don't do this if childEntry == 0, why is it like this?

  child = fatSystem.openDirectory(&fatSystem, "child");
  DirectoryEntry *grandchildEntry = fatSystem.readDirectory(child);
  assertIntNotEquals((uintptr_t)grandchildEntry, 0);
  assertString(grandchildEntry->filename, "gc");
  grandchildEntry = fatSystem.readDirectory(child);
  assertInt((int)grandchildEntry, 0);
  fatSystem.closeDirectory(child);
//   directoryEntry_free(grandchildEntry); //FIXME: Don't do this if childEntry == 0
}
//FIXME: Should probably work
IGNORE_TEST(read_write, createDirectory_createWhileParentOpen_readChildSuccessfully){
  Directory *parent = fatSystem.createDirectory(&fatSystem, "parent");
  Directory *sub = fatSystem.createDirectory(&fatSystem, "parent/sub");
  fatSystem.closeDirectory(sub);

  DirectoryEntry *entry = fatSystem.readDirectory(parent);
  if(assertIntNotEquals((uintptr_t)entry, 0)){
    directoryEntry_free(entry);
  }
  fatSystem.closeDirectory(parent);
}
TEST(read_write, createDirectory_createSiblingsWithoutClose_readSiblingsSuccessfully){
  Directory *parent = fatSystem.createDirectory(&fatSystem, "parent");
  fatSystem.closeDirectory(parent);

  Directory *sub1 = fatSystem.createDirectory(&fatSystem, "parent/sub");
  Directory *sub2 = fatSystem.createDirectory(&fatSystem, "parent/sub2");
  fatSystem.closeDirectory(sub1);
  fatSystem.closeDirectory(sub2);

  parent = fatSystem.openDirectory(&fatSystem, "parent");
  DirectoryEntry *entry1 = fatSystem.readDirectory(parent);
  DirectoryEntry *entry2 = fatSystem.readDirectory(parent);

  if(assertIntNotEquals((uintptr_t)entry1, 0)){
    directoryEntry_free(entry1);
  }
  if(assertIntNotEquals((uintptr_t)entry2, 0)){
    directoryEntry_free(entry2);
  }
  fatSystem.closeDirectory(parent);
}
TEST(read_write, createTree){
  createTreeHelper("/", 5);
  verifyTree("/", 5);
}

TEST(read_write, readDirectory_readRootWith3Files_find3Files){
  for(int i = 0; i < 3; i++){
    char name[20];
    sprintf(name, "t%d.txt", i);
    File *file = fatSystem.createFile(&fatSystem, name);
    fatSystem.closeFile(file);
  }

  Directory *root = fatSystem.openDirectory(&fatSystem, "/");

  uint32_t found = 0;
  for(int i = 0; i < 3; i++){
    DirectoryEntry *entry = fatSystem.readDirectory(root);
    uint32_t n = entry->filename[1] - '0';
    found |= 1 << n;
    entry->filename[1] = 'x';
    assertString(entry->filename, "tx.txt");

    directoryEntry_free(entry);
  }

  assertInt(found, 0b111);

  fatSystem.closeDirectory(root);
}

TEST(read_write, readDirectory_trailingSlash){
  Directory *dir = fatSystem.createDirectory(&fatSystem, "dir");
  fatSystem.closeDirectory(dir);

  dir = fatSystem.openDirectory(&fatSystem, "dir/");
  assertString(dir->name, "dir");
  fatSystem.closeDirectory(dir);
}



TEST(read_write, createFile_correctName){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  assertString(file->name, "test.txt");
  fatSystem.closeFile(file);
}
TEST(read_write, createFile_reservedNames_NullPointers){
  char *reservedDirectoryNames[] = {"", " ", "  ", ".", "..", "...", "/", "//", "///"};
  for(uint32_t i = 0; i < sizeof(reservedDirectoryNames) / sizeof(uint8_t*); i++){
    File *file = fatSystem.createFile(&fatSystem, reservedDirectoryNames[i]);
    if(!assertInt((uintptr_t)file, 0)){
      fatSystem.closeFile(file);
      break;
    }       
  } 
}
TEST(read_write, createFile_rootIndicator_succes){
  File *file = fatSystem.createFile(&fatSystem, "/test.txt");
  assertString(file->name, "test.txt");
  fatSystem.closeFile(file);
}
TEST(read_write, createFile_multipleFilesSameName_NullPointer){
  File *f1 = fatSystem.createFile(&fatSystem, "test.txt");
  File *f2 = fatSystem.createFile(&fatSystem, "test.txt");

  assertInt((uint32_t)f2, 0);
  fatSystem.closeFile(f1);
  if(f2){
    fatSystem.closeFile(f2);
  }
}
TEST(read_write, createFile_nonexistentParentDirectory_NullPointer){
  File *f1 = fatSystem.createFile(&fatSystem, "parent/test.txt");
  if(!assertInt((uint32_t)f1, 0)){
    fatSystem.closeFile(f1);
  }
}
TEST(read_write, createFile_newFileReadIsEmpty){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  char buffer[512];
  int length = fatSystem.readFile(file, buffer, sizeof(buffer));
  assertInt(length, 0);

  fatSystem.closeFile(file);
}

TEST(read_write, createFile_createInDirectory_sucess){
  createTreeHelper("/", 3);

  File *f1 = fatSystem.createFile(&fatSystem, "/c0/c0/c0/test.txt");
  fatSystem.closeFile(f1);
  f1 = fatSystem.openFile(&fatSystem, "/c0/c0/c0/test.txt");
  assertString(f1->name, "test.txt");
  fatSystem.closeFile(f1);

  f1 = fatSystem.createFile(&fatSystem, "/c0/c0/test.txt");
  fatSystem.closeFile(f1);
  f1 = fatSystem.openFile(&fatSystem, "/c0/c0/test.txt");
  assertString(f1->name, "test.txt");
  fatSystem.closeFile(f1);

  f1 = fatSystem.createFile(&fatSystem, "/c0/test.txt");
  fatSystem.closeFile(f1);
  f1 = fatSystem.openFile(&fatSystem, "/c0/test.txt");
  assertString(f1->name, "test.txt");
  fatSystem.closeFile(f1);

  f1 = fatSystem.createFile(&fatSystem, "/test.txt");
  fatSystem.closeFile(f1);
  f1 = fatSystem.openFile(&fatSystem, "/test.txt");
  assertString(f1->name, "test.txt");
  fatSystem.closeFile(f1);


}

TEST(read_write, removeFile_removeExisting_fileRemoved){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  fatSystem.closeFile(file);

  fatSystem.remove(&fatSystem, "test.txt");

  Directory *root = fatSystem.openDirectory(&fatSystem, "/");
  DirectoryEntry *child = fatSystem.readDirectory(root);
  assertInt((uint32_t)child, 0);

  fatSystem.closeDirectory(root);
  //   directoryEntry_free(child);
}
TEST(read_write, removeFile_removeNonExistingFile_Error){
  uint32_t status = fatSystem.remove(&fatSystem, "test.txt");
  assertInt(status, 0);
}

TEST(read_write, removeFile_removeMiddleFileOf3_2FilesLeft){
  for(int i = 0; i < 3; i++){
    char res[50];
    sprintf(res, "test%d.txt", i);
    File *file = fatSystem.createFile(&fatSystem, res);
    fatSystem.closeFile(file);
  }
  fatSystem.remove(&fatSystem, "test1.txt");

  Directory *root = fatSystem.openDirectory(&fatSystem, "/");
  for(int i = 0; i < 3; i++){
    if(i == 1) continue;

    DirectoryEntry *child = fatSystem.readDirectory(root);
    char expected[50];
    sprintf(expected, "test%d.txt", i);
    assertString(child->filename, expected);
    directoryEntry_free(child);
  }
  fatSystem.closeDirectory(root);
}
TEST(read_write, openFile_correctName){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  fatSystem.closeFile(file);


  file = fatSystem.openFile(&fatSystem, "test.txt");
  assertString(file->name, "test.txt");

  fatSystem.closeFile(file);
}

TEST(read_write, writeReadFile_readEqualsWriteSmall){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  char buffer[] = "Hello World!\n";
  fatSystem.writeFile(file, buffer, sizeof(buffer));
  fatSystem.closeFile(file);

  file = fatSystem.openFile(&fatSystem, "test.txt");
  char result[sizeof(buffer)];
  fatSystem.readFile(file, result, sizeof(result));

  assertArray(result, sizeof(result), buffer, sizeof(buffer));

  fatSystem.closeFile(file);
}
TEST(read_write, writeReadFile_readEqualsWriteBig){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  char buffer[] = "Hello World!\n";
  for(int i = 0; i < 1000; i++){
    fatSystem.writeFile(file, buffer, sizeof(buffer));
  }
  fatSystem.closeFile(file);

  file = fatSystem.openFile(&fatSystem, "test.txt");
  char result[sizeof(buffer)];
  for(int i = 0; i < 1000; i++){
    fatSystem.readFile(file, result, sizeof(result));
    if(!assertArray(result, sizeof(result), buffer, sizeof(buffer)));
  }

  fatSystem.closeFile(file);
}

IGNORE_TEST(read_write, writeFile_writeLongThenShort_fileGetsSmaller){
  File *file = fatSystem.createFile(&fatSystem, "test.txt");
  char buffer[] = "Hello World!\n";
  for(int i = 0; i < 1000; i++){
    fatSystem.writeFile(file, buffer, sizeof(buffer));
  }
  fatSystem.closeFile(file);


  file = fatSystem.openFile(&fatSystem, "test.txt");
  fatSystem.writeFile(file, buffer, sizeof(buffer));
  fatSystem.closeFile(file);

  file = fatSystem.openFile(&fatSystem, "test.txt");
  char result[sizeof(buffer)];
  fatSystem.readFile(file, result, sizeof(result));
  assertArray(result, sizeof(result), buffer, sizeof(buffer));
  uint32_t length = fatSystem.readFile(file, result, sizeof(result));
  assertInt(length, 0);

  fatSystem.closeFile(file);
}




TEST(string, equalPrefixLength_ZeroEqual){
  assertInt(equalPrefixLength("abcdef", "xyz"), 0);
}
TEST(string, equalPrefixLength_empty){
  assertInt(equalPrefixLength("", ""), 0);
  assertInt(equalPrefixLength("", "abcdef"), 0);
  assertInt(equalPrefixLength("abcdef", ""), 0);
}
TEST(string, equalPrefixLength_1Equal){
  assertInt(equalPrefixLength("abc", "acd"), 1);
}
TEST(string, equalPrefixLength_2Equal){
  assertInt(equalPrefixLength("abcfefe", "abd"), 2);
}


TEST(string, parentFromPath_noParent){
  char *result = parentFromPath("file.txt");
  assertString(result, "");
  kfree(result);
}
TEST(string, parentFromPath_noFile){
  char *result = parentFromPath("dir/");
  assertString(result, "dir");
  kfree(result);
}
TEST(string, parentFromPath_noPath){
  char *result = parentFromPath("");
  assertString(result, "");
  kfree(result);
}
TEST(string, parentFromPath_pathAndFile){
  char *result = parentFromPath("dir/file.txt");
  assertString(result, "dir");
  kfree(result);
}
TEST(string, parentFromPath_longPathAndFile){
  char *result = parentFromPath("dir1/dir2/dir3/file.txt");
  assertString(result, "dir1/dir2/dir3");
  kfree(result);
}

TEST(string, fileNameFromPath_noParent){
  char *result = fileNameFromPath("file.txt");
  assertString(result, "file.txt");
  kfree(result);
}
TEST(string, fileNameFromPath_noFile){
  char *result = fileNameFromPath("dir/");
  assertString(result, "");
  kfree(result);
}
TEST(string, fileNameFromPath_noPath){
  char *result = fileNameFromPath("");
  assertString(result, "");
  kfree(result);
}
TEST(string, fileNameFromPath_pathAndFile){
  char *result = fileNameFromPath("dir/file.txt");
  assertString(result, "file.txt");
  kfree(result);
}
TEST(string, fileNameFromPath_longPathAndFile){
  char *result = fileNameFromPath("dir1/dir2/dir3/file.txt");
  assertString(result, "file.txt");
  kfree(result);
}


TEST(string, removeTrailingSpaces_spaces){
  char arr[] = "abcdef    ";
  char result[sizeof(arr)];
  uint32_t newLength = removeTrailingSpaces((uint8_t*)arr, sizeof(arr) - 1, (uint8_t*)result);
  result[newLength] = 0;
  assertString(result, "abcdef");
}
TEST(string, removeTrailingSpaces_noSpaces){
  char arr[] = "abcdef";
  char result[sizeof(arr)];
  uint32_t newLength = removeTrailingSpaces((uint8_t*)arr, sizeof(arr) - 1, (uint8_t*)result);
  result[newLength] = 0;
  assertString(result, "abcdef");
}
TEST(string, removeTrailingSpaces_onlySpaces){
  char arr[] = "   ";
  char result[sizeof(arr)];
  uint32_t newLength = removeTrailingSpaces((uint8_t*)arr, sizeof(arr) - 1, (uint8_t*)result);
  result[newLength] = 0;
  assertString(result, "");
}
TEST(string, removeTrailingSpaces_empy){
  char arr[] = "";
  char result[sizeof(arr)];
  uint32_t newLength = removeTrailingSpaces((uint8_t*)arr, sizeof(arr) - 1, (uint8_t*)result);
  result[newLength] = 0;
  assertString(result, "");
}

TEST(string, strToFileName_nameAndExtension){
  char result[11];
  strToFilename("file.txt", result);
  assertArray(result, 11, "FILE    TXT", 11);
}
TEST(string, strToFileName_noExtension){
  char result[11];
  strToFilename("file", result);
  assertArray(result, 11, "FILE       ", 11);
}
TEST(string, strToFileName_onlyExtension){
  char result[11];
  strToFilename(".txt", result);
  assertArray(result, 11, "        TXT", 11);
}
TEST(string, strToFileName_empty){
  char result[11];
  strToFilename("", result);
  assertArray(result, 11, "           ", 11);
}
TEST(string, strToFileName_dot){
  char result[11];
  strToFilename(".", result);
  assertArray(result, 11, "           ", 11);
}
TEST(string, strToFileName_15charsLong){
  char result[11];
  FatStatus status = strToFilename("abcdefghijklmno", result);
  assertInt(status, FatInvalidFileName);
}

TEST(string, getFileName_nameAndExtension){
  char result[13];
  FatDirectoryEntry entry;
  memcpy(entry.fileName, "FILE    TXT", 11);
  getFileName(entry, (uint8_t*)result);
  assertString(result, "file.txt");
}
TEST(string, getFileName_onlyName){
  char result[13];
  FatDirectoryEntry entry;
  memcpy(entry.fileName, "FILE       ", 11);
  getFileName(entry, (uint8_t*)result);
  assertString(result, "file");
}
TEST(string, getFileName_onlyExtention){
  char result[13];
  FatDirectoryEntry entry;
  memcpy(entry.fileName, "        TXT", 11);
  getFileName(entry, (uint8_t*)result);
  assertString(result, ".txt");
}
TEST(string, getFileName_empty){
  char result[13];
  FatDirectoryEntry entry;
  memcpy(entry.fileName, "           ", 11);
  getFileName(entry, (uint8_t*)result);
  assertString(result, "");
}

END_TESTS

