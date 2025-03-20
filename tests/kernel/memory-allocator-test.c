#include "testrunner.h"
#include "kernel/memory.h"
#include "stdlib.h"
#include "stdarg.h"

static uint8_t ram[4096];
static uint8_t before[sizeof(ram)];

static unsigned memorySize;
static Memory *memory;

TEST_GROUP_SETUP(dtg){}
TEST_GROUP_TEARDOWN(dtg){}

TEST_GROUP_SETUP(pageMemory){
   memorySize = sizeof(ram);
   memory = memory_new(&ram, memorySize);
   memcpy(before, ram, sizeof(ram));
}

TEST_GROUP_TEARDOWN(pageMemory){
}

bool isCrossingBoundary(uintptr_t address, unsigned int size, unsigned int boundary){
   uintptr_t modStart = address % boundary;
   uintptr_t modEnd = modStart + size;

   return modEnd > boundary;
}

bool isOverlaping(MemoryArea area1, MemoryArea area2){
   return area1.start + area1.size > area2.start
      && area1.start < area2.start + area2.size;
}

bool isAnyOverlaping(MemoryArea *areas, int count){
   for(int i = 0; i < count; i++){
      for(int j = i + 1; j < count; j++){
         if(isOverlaping(areas[i], areas[j])){
            return true;
         }
      }
   }
   return false;
}

bool isInMemory(uintptr_t start, unsigned int size, uintptr_t memoryStart, unsigned int memorySize){
   return start >= (uintptr_t)memoryStart && start + size <= memoryStart + memorySize;
}
bool isInDefaultMemory(uintptr_t start, unsigned int size){
   return isInMemory(start, size, (uintptr_t)ram, memorySize);
}
bool isAreaInMemory(MemoryArea area, uintptr_t memoryStart, unsigned int memorySize){
   return isInMemory((uintptr_t)area.start, area.size, memoryStart, memorySize);
}
bool isAreaInDefaultMemory(MemoryArea area){
   return isInDefaultMemory((uintptr_t)area.start, area.size);
}

unsigned int mallocMany(unsigned int *sizes, unsigned int count, MemoryArea *result){
   for(unsigned int i = 0; i < count; i++){
      result[i] = (MemoryArea){
         .start = memory_malloc(memory, sizes[i]),
            .size = sizes[i]
      };
      if(!result[i].start){
         return i;
      }
   }
   return count;
}

unsigned int malloccoMany(unsigned int *sizes, unsigned int count, unsigned int alignment, unsigned int boundary, MemoryArea *result){
   for(unsigned int i = 0; i < count; i++){
      result[i] = (MemoryArea){
         .start = memory_mallocco(memory, sizes[i], alignment, boundary),
            .size = sizes[i]
      };
      if(!result[i].start){
         return i;
      }
   }
   return count;
}

bool isMemoryUnchanged(){
   for(unsigned int i = 0; i < sizeof(ram); i++){
      if(ram[i] != before[i]){
         return false; 
      }
   }
   return true;
}

TESTS

TEST(pageMemory, allocate4000_success_validAddress){
   unsigned int size = 4000;
   uintptr_t mem = (uintptr_t)memory_malloc(memory, size);

   assertIntNotEquals(mem, 0);
   assertInt(isInDefaultMemory(mem, size), true);
}

TEST(pageMemory, allocate4097_failed_noChangesToMemory){
   void *result = memory_malloc(memory, sizeof(ram) + 1);

   assertInt((uintptr_t)result, 0);
   assertInt(isMemoryUnchanged(), true);
}

TEST(pageMemory, allocateThirdOfSize2000_failed){
   memory_malloc(memory, sizeof(ram) + 1);
   memory_malloc(memory, sizeof(ram) + 1);

   void *result = memory_malloc(memory, sizeof(ram) + 1);
   assertInt((uintptr_t)result, 0);
}

TEST(pageMemory, allocate2ofSize30_success_nonOverlaping){
   unsigned int sizes[] = {30, 30};
   MemoryArea areas[2]; mallocMany(sizes, 2, areas);

   assertInt(isAreaInDefaultMemory(areas[0]), true);
   assertInt(isAreaInDefaultMemory(areas[1]), true);
   assertInt(isAnyOverlaping(areas, 2), false);
}

TEST(pageMemory, allocateOfSize1UntilFull_nonOverlaping){
   unsigned int sizes[4096];
   for(int i = 0; i < 4096; i++){
      sizes[i] = 1;
   }
   MemoryArea areas[4096]; 

   unsigned int count = mallocMany(sizes, 4096, areas);

   for(unsigned int i = 0; i < count; i++){
      if(!assertInt(isAreaInDefaultMemory(areas[i]), true)){
         break;
      }
   }
   assertInt(isAnyOverlaping(areas, count), false);
}

TEST(pageMemory, release_released4000CanBeReallocated){
   unsigned int size = 4000;
   void *p1 = memory_malloc(memory, size); 
   memory_free(memory, p1);

   void *p2 = memory_malloc(memory, size);

   assertIntNotEquals((uintptr_t)p2, 0);
   assertInt(isInDefaultMemory((uintptr_t)p2, size), true);
}

TEST(pageMemory, release_released3000CanBeReallocated){
   unsigned int size = 3000;
   void *p1 = memory_malloc(memory, size); 
   memory_free(memory, p1);

   void *p2 = memory_malloc(memory, size);

   assertIntNotEquals((uintptr_t)p2, 0);
   assertInt(isInDefaultMemory((uintptr_t)p2, size), true);
}

TEST(pageMemory, release_release3OfSize1000_allocateOneOfSize4000_success){
   unsigned int smallSize = 1000, bigSize = 4000;

   void *p1 = memory_malloc(memory, smallSize); 
   void *p2 = memory_malloc(memory, smallSize); 
   void *p3 = memory_malloc(memory, smallSize); 
   memory_free(memory, p1);
   memory_free(memory, p2);
   memory_free(memory, p3);

   void *pbig = memory_malloc(memory, bigSize);

   assertIntNotEquals((uintptr_t)pbig, 0);
   assertInt(isInDefaultMemory((uintptr_t)pbig, bigSize), true);
}

//Should proably remove when running in real OS
TEST(pageMemory, releaseFirst_noNullPointerReferences){
   uint8_t tmp[16];
   memcpy(tmp, 0, sizeof(tmp));
   memset(0, 0, sizeof(tmp));

   unsigned int size = 500;
   void *p1 = memory_malloc(memory, size); 

   memory_free(memory, p1);

   uint8_t *ptr = 0;
   for(int i = 0; i < 16; i++){
      if(!assertInt(ptr[i], 0)){
         break;
      }
   }
   memcpy(0, tmp, sizeof(tmp));
}

TEST(pageMemory, release_releaseUnallocated_memoryNotModified){
   uint8_t before[sizeof(ram)];
   memcpy(before, ram, sizeof(ram));

   memory_free(memory, &ram[3000]);

   for(unsigned int i = 0; i < sizeof(ram); i++){
      if(!assertInt(before[i], ram[i])){
         break;
      }
   }
}

TEST(pageMemory, calloc4000_success_allZeroes){
   unsigned int size = 4000;

   uint32_t *p1 = memory_malloc(memory, size);
   memset(p1, 0xFF, size);
   memory_free(memory, p1);

   uint32_t *result = memory_calloc(memory, size);

   assertIntNotEquals((uintptr_t)result, 0);
   for(unsigned int i = 0; i < size / 4; i++){
      if(!assertInt(result[i], 0)){
         break;
      }
   }
}

//Should proably remove when running in real OS
TEST(pageMemory, calloc4097_failed_noChangesToAddress0){
   uint32_t tmp;
   memcpy(&tmp, 0, sizeof(tmp));
   memset(0, 0xFF, sizeof(tmp));

   void *result = memory_calloc(memory, 4097);

   assertInt((uintptr_t)result, 0);
   uint32_t nullValue;
   memcpy(&nullValue, 0, sizeof(nullValue));
   assertInt(nullValue, 0xFFFFFFFF);

   memcpy(0, &tmp, sizeof(tmp));
}

TEST(pageMemory, mallocco_8ByteAllignedNoBoundarySize63_success_alligned){
   uintptr_t result = (uintptr_t)memory_mallocco(memory, 63, 8, 0);

   assertIntNotEquals(result, 0);
   assertInt(result % 8, 0);
   assertInt(isInDefaultMemory(result, 63), true);
}

TEST(pageMemory, mallocco_64ByteAllignedNoBoundarySize63_success_alligned){
   uintptr_t result = (uintptr_t)memory_mallocco(memory, 63, 64, 0);

   assertIntNotEquals(result, 0);
   assertInt(result % 64, 0);
   assertInt(isInDefaultMemory(result, 63), true);
}

TEST(pageMemory, mallocco_8ByteAllignedNoBoundarySize63TilFailure_success_alligned_nonOverlaping){
   unsigned int bufferSize = 4096, size = 63, alignment = 8, boundary = 0;
   unsigned int sizes[bufferSize];
   for(unsigned int i = 0; i < bufferSize; i++){
      sizes[i] = size;
   }
   MemoryArea areas[bufferSize]; 

   unsigned int count = malloccoMany(sizes, bufferSize, alignment, boundary, areas);

   assertInt(isAnyOverlaping(areas, count), false);
   for(unsigned int i = 0; i < count; i++){
      uintptr_t addr = (uintptr_t)areas[i].start;
      if(!assertInt(addr % alignment, 0)){
         break;
      }
      if(!assertInt(isAreaInDefaultMemory(areas[i]), true)){
         break;
      }
   }
}

TEST(pageMemory, mallocco_8ByteAllignedNoBoundarySize1001TilFailure_success_alligned_nonOverlaping){
   const unsigned int bufferSize = 4096, size = 1001, alignment = 8, boundary = 0;
   unsigned int sizes[bufferSize];
   for(unsigned int i = 0; i < bufferSize; i++){
      sizes[i] = size;
   }
   MemoryArea areas[bufferSize]; 

   unsigned int count = malloccoMany(sizes, bufferSize, alignment, boundary, areas);

   assertInt(isAnyOverlaping(areas, count), false);
   for(unsigned int i = 0; i < count; i++){
      uintptr_t addr = (uintptr_t)areas[i].start;
      if(!assertInt(addr % alignment, 0)){
         break;
      }
      if(!assertInt(isAreaInDefaultMemory(areas[i]), true)){
         break;
      }
   }
}

TEST(pageMemory, mallocco_noAlignmentNoBoundarySize8_sameCountAsMalloc){
   unsigned const int size = 8;
   void *ptrBuffer[sizeof(ram) / size];

   unsigned int count = 0;
   for(; count < sizeof(ptrBuffer) / sizeof(void *); count++){
      ptrBuffer[count] = memory_malloc(memory, size);
      if(!ptrBuffer[count]){
         break;
      }
   }
   for(unsigned int i = 0; i < count; i++){
      memory_free(memory, ptrBuffer[i]);
   }

   unsigned int mallocoCount = 0;
   for(; mallocoCount < count; mallocoCount++){
      if(!memory_mallocco(memory, size, 1, 0)){
         break;
      }
   }

   assertInt(mallocoCount, count);
}

TEST(pageMemory, mallocco_noAlignment64ByteBoundarySize60){
   unsigned const int size = 60, alignment = 1, boundary = 64;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertIntNotEquals(result, 0);
   assertInt(isInDefaultMemory(result, size), true);
   assertInt(isCrossingBoundary(result, size, boundary), false);
}

TEST(pageMemory, mallocco_noAlignment8ByteBoundarySize8){
   unsigned const int size = 8, alignment = 1, boundary = 8;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertIntNotEquals(result, 0);
   assertInt(isInDefaultMemory(result, size), true);
   assertInt(isCrossingBoundary(result, size, boundary), false);
}

TEST(pageMemory, mallocco_alignment8Boundary32Size16_success){
   unsigned const int size = 16, alignment = 8, boundary = 32;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertIntNotEquals(result, 0);
   assertInt(isInDefaultMemory(result, size), true);
   assertInt(isCrossingBoundary(result, size, boundary), false);
   assertInt(result % alignment, 0);
}

TEST(pageMemory, mallocco_alignment32Boundary16Size16_success){
   unsigned const int size = 16, alignment = 32, boundary = 16;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertIntNotEquals(result, 0);
   assertInt(isInDefaultMemory(result, size), true);
   assertInt(isCrossingBoundary(result, size, boundary), false);
   assertInt(result % alignment, 0);
}

TEST(pageMemory, mallocco_alignmentNotPowerOf2_failed){
   unsigned const int size = 256, alignment = 69, boundary = 0;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertInt(result, 0);
   assertInt(isMemoryUnchanged(), true);
}

TEST(pageMemory, mallocco_boundaryNotPowerOf2_failed){
   unsigned const int size = 12, alignment = 1, boundary = 69;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertInt(result, 0);
   assertInt(isMemoryUnchanged(), true);
}

TEST(pageMemory, mallocco_sizeGreaterThanBoundary_failed){
   unsigned const int size = 256, alignment = 1, boundary = 128;

   uintptr_t result = (uintptr_t)memory_mallocco(memory, size, alignment, boundary);

   assertInt(result, 0);
   assertInt(isMemoryUnchanged(), true);
}

TEST(pageMemory, callocco_alignment8Boundary32Size32_success_allZeroes){
   unsigned const int size = 32, alignment = 8, boundary = 32;

   uintptr_t result = (uintptr_t)memory_callocco(memory, size, alignment, boundary);

   assertIntNotEquals(result, 0);

   uint32_t *ptr = (uint32_t *)result;
   for(unsigned int i = 0; i < size / 4; i++){
      if(!assertInt(ptr[i], 0)){
         break;
      }
   }
}

TEST(pageMemory, append_allocate4000TwiceWithExtraRam){
   unsigned const int size = 4000;
   uint8_t extraRam[4096];
   memory_append(memory, extraRam, sizeof(extraRam));
   memory_malloc(memory, size);

   uintptr_t result = (uintptr_t)memory_malloc(memory, size);

   assertIntNotEquals(result, 0);
   assertInt(isInMemory(result, size, (uintptr_t)extraRam, sizeof(extraRam)), true);
}

TEST(dtg, appendContinuous_allowsForBiggerAllocation){
   unsigned const int memSize1 = 2048, memSize2 = 2048, mallocSize = 4000;
   uint8_t ram[memSize1 + memSize2];

   Memory *memory = memory_new(ram, memSize1);
   memory_append(memory, ram + memSize1, memSize2);

   uintptr_t result = (uintptr_t)memory_malloc(memory, mallocSize);

   assertIntNotEquals(result, 0);
   assertInt(isInMemory(result, mallocSize, (uintptr_t)ram, sizeof(ram)), true);
}

IGNORE_TEST(dtg, appendToMakeContinuous_appendInReverse_allowsForBiggerAllocation){
   unsigned const int memSize1 = 1000, memSize2 = 1000, memSize3 = 1000, mallocSize = 2950;
   uint8_t ram[memSize1 + memSize2 + memSize3];

   Memory *memory = memory_new(ram + memSize1 + memSize2, memSize3);
   memory_append(memory, ram + memSize1, memSize2);
   memory_append(memory, ram, memSize1);

   uintptr_t result = (uintptr_t)memory_malloc(memory, mallocSize);

   assertIntNotEquals(result, 0);
   assertInt(isInMemory(result, mallocSize, (uintptr_t)ram, sizeof(ram)), true);
}

TEST(dtg, appendToMakeContinuous_afterAppendingToMakeNotContinuous_allowsForBiggerAllocation){
   unsigned const int memSize1 = 1000, memSize2 = 1000, memSize3 = 1000, mallocSize = 2950;
   uint8_t ram[memSize1 + memSize2 + memSize3];

   Memory *memory = memory_new(ram, memSize1);
   memory_append(memory, ram + memSize1 + memSize2, memSize3);
   memory_append(memory, ram + memSize1, memSize2);

   uintptr_t result = (uintptr_t)memory_malloc(memory, mallocSize);

   assertIntNotEquals(result, 0);
   assertInt(isInMemory(result, mallocSize, (uintptr_t)ram, sizeof(ram)), true);
}

// Order: |1|4|3|2|
TEST(dtg, appendToMakeContinuous_rightEdge_allowsForBiggerAllocation){
   unsigned const int memSize1 = 1000, memSize2 = 1000, memSize3 = 1000, memSize4 = 1000, mallocSize = 3950;
   uint8_t ram[memSize1 + memSize2 + memSize3 + memSize4];

   Memory *memory = memory_new(ram, memSize1);
   memory_append(memory, ram + memSize1 + memSize2 + memSize3, memSize4);
   memory_append(memory, ram + memSize1 + memSize2, memSize3);
   memory_append(memory, ram + memSize1, memSize2);

   uintptr_t result = (uintptr_t)memory_malloc(memory, mallocSize);

   assertIntNotEquals(result, 0);
   assertInt(isInMemory(result, mallocSize, (uintptr_t)ram, sizeof(ram)), true);
}

// Order: |2|3|4|2|
TEST(dtg, appendToMakeContinuous_leftEdge_allowsForBiggerAllocation){
   unsigned const int memSize1 = 1000, memSize2 = 1000, memSize3 = 1000, memSize4 = 1000, mallocSize = 3950;
   uint8_t ram[memSize1 + memSize2 + memSize3 + memSize4];

   Memory *memory = memory_new(ram, memSize1);
   memory_append(memory, ram + memSize1 + memSize2 + memSize3, memSize4);
   memory_append(memory, ram + memSize1, memSize2);
   memory_append(memory, ram + memSize1 + memSize2, memSize3);

   uintptr_t result = (uintptr_t)memory_malloc(memory, mallocSize);

   assertIntNotEquals(result, 0);
   assertInt(isInMemory(result, mallocSize, (uintptr_t)ram, sizeof(ram)), true);
}
END_TESTS
