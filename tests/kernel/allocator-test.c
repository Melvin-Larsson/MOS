#include "testrunner.h"
#include "kernel/allocator.h"
#include "stdio.h"

static Allocator *allocator;
static int size;
static int address;


TEST_GROUP_SETUP(size1){
   address = 0;
   size = 100000;
   allocator = allocator_init(address, size);
}
TEST_GROUP_TEARDOWN(size1){
    allocator_free(allocator);
}

TEST_GROUP_SETUP(size2){
   address = 0;
   size = 10;
   allocator = allocator_init(address, size);
}
TEST_GROUP_TEARDOWN(size2){
    allocator_free(allocator);
}

static int isAllocatedAreasOverlaping(AllocatedArea *areas, int count){
   for(int i = 0; i < count; i++){
      for(int j = i + 1; j < count; j++){
         if(areas[i].address < areas[j].address + areas[j].size &&
               areas[i].address + areas[i].size > areas[j].address){

            printf("%d %d | %d %d\n", areas[i].address, areas[i].size, areas[j].address, areas[j].size);
            return 1;
         }
      }
   }
   return 0;
}

static void allocateMultiple(int count, int size, AllocatedArea *results){
   for(int i = 0; i < count; i++){
      results[i] = allocator_get(allocator, size);
   }
}


TESTS

TEST(size1, get100_gets100){
//    AllocatedArea area = allocator_get(allocator, 100);
//    assertInt(area.size, 100);
}
TEST(size1, get200_gets200){
   AllocatedArea area = allocator_get(allocator, 200);
   assertInt(area.size, 200);
}
TEST(size1, get100ThenGet100_getsDifferent100s){
   AllocatedArea area1 = allocator_get(allocator, 100);
   AllocatedArea area2 = allocator_get(allocator, 100);

   assertIntNotEquals(area1.address, area2.address);
}

TEST(size1, get100ds_doesNotOverlap){
   AllocatedArea areas[size/100];
   allocateMultiple(size/100, 100, areas);
   assertInt(isAllocatedAreasOverlaping(areas, size/100), 0);
}

TEST(size1, get200ds_doesNotOverlap){
   AllocatedArea areas[size/200];
   allocateMultiple(size/200, 200, areas);
   assertInt(isAllocatedAreasOverlaping(areas, size/200), 0);
}

TEST(size1, getTotalSizePlus1_returnSize0){
   AllocatedArea area = allocator_get(allocator, size + 1); 
   assertInt(area.size, 0);
}
TEST(size1, getTotalSizePlus100_returnSize0){
   AllocatedArea area = allocator_get(allocator, size + 100); 
   assertInt(area.size, 0);
}
TEST(size2, getTotalSize2Plus1_returnsSize0){
   AllocatedArea area = allocator_get(allocator, size + 1); 
   assertInt(area.size, 0);
}
TEST(size1, getTotalSizePlusOneIn2Gets_returnsSize0On2nd){
   allocator_get(allocator, size / 2);
   AllocatedArea area = allocator_get(allocator, size / 2+ 1); 
   assertInt(area.size, 0);
}

TEST(size1, getTotalSizeMultipleAllocators_returnsTotalSize){
   allocator_get(allocator, size);

   Allocator *allocator2 = allocator_init(0, size);
   AllocatedArea area2 = allocator_get(allocator2, size);

   assertInt(area2.size, size);

   allocator_free(allocator2);
}

TEST(size1, getTotalSizeThenRelease100ThenGet100_getsTheReleased100){
   allocator_get(allocator, size);
   allocator_release(allocator, address, 100);

   AllocatedArea area = allocator_get(allocator, 100);

   assertInt(area.address, address);
   assertInt(area.size, 100);
}
TEST(size1, getTotalSizeThenRelease100ThenGet200_getsSize0){
   allocator_get(allocator, size);
   allocator_release(allocator, address, 100);

   AllocatedArea area = allocator_get(allocator, 200);

   assertInt(area.size, 0);
}
TEST(size1, getTotalSizeThenRelease100TwiceThenGet200_getsFirstRelease){
   allocator_get(allocator, size);
   allocator_release(allocator, address, 100);
   allocator_release(allocator, address + 100, 100);

   AllocatedArea area = allocator_get(allocator, 200);

   assertInt(area.address, address);
   assertInt(area.size, 200);
}
TEST(size1, getTotalSizeThenRelease100To200ThenRelease0To100ThenGet200_getsArea){
   allocator_get(allocator, size);
   allocator_release(allocator, address + 100, 100);
   allocator_release(allocator, address, 100);

   AllocatedArea area = allocator_get(allocator, 200);

   assertInt(area.address, address);
   assertInt(area.size, 200);
}
TEST(size1, getTotalSizeThenReleaseLast100ThenReleaseSecondLast100ThenGet200_getsLast200){
   allocator_get(allocator, size);
   allocator_release(allocator, size - 100, 100);
   allocator_release(allocator, size - 200, 100);

   AllocatedArea area = allocator_get(allocator, 200);

   assertInt(area.address, size - 200);
   assertInt(area.size, 200);
}

TEST(size1, getToalSizeThenReleaseEverythingBut100To200ThenRelease100To200ThenGetTotal_getsTotalSize){
   allocator_get(allocator, size);
   allocator_release(allocator, address, 100);
   allocator_release(allocator, address + 200, size - 200);
   allocator_release(allocator, address + 100, 100);

   AllocatedArea area = allocator_get(allocator, size);

   assertInt(area.address, address);
   assertInt(area.size, size);
}

TEST(size1, reserve100ThenGetTotalSize_getsNothing){
   allocator_markAsReserved(allocator, address, 100);

   AllocatedArea area = allocator_get(allocator, size);

   assertInt(area.size, 0);
}
TEST(size1, reserve50ThenGetAllBut50_getsArea){
   allocator_markAsReserved(allocator, address, 50);

   AllocatedArea area = allocator_get(allocator, size - 50);

   assertInt(area.size, size - 50);
}
TEST(size1, reserve50To100GetAllBut50_doesNotGetArea){
   allocator_markAsReserved(allocator, address + 50, 50);

   AllocatedArea area = allocator_get(allocator, size - 50);

   assertInt(area.size, 0);
}
TEST(size1, reserveLast100ThenGetTotalSize_doesNotGetArea){
   allocator_markAsReserved(allocator, size - 100, 100);

   AllocatedArea area = allocator_get(allocator, size);

   assertInt(area.size, 0);
}
TEST(size1, reserveMultipleAreas_noneAvailiableForGet){
   allocator_get(allocator, size);
   allocator_release(allocator, address, 50);
   allocator_release(allocator, address + 100, 50);
   allocator_release(allocator, address + 200, 50);
   allocator_markAsReserved(allocator, address, 250);

   AllocatedArea area = allocator_get(allocator, 1);

   assertInt(area.size, 0);
}

TEST(size1, reserveMultiplePartialGetUnreservedPartOfPartial_areaAvailiable){
   allocator_get(allocator, size);
   allocator_release(allocator, address, 50);
   allocator_release(allocator, address + 100, 50);
   allocator_release(allocator, address + 200, 50);
   allocator_markAsReserved(allocator, address + 25, 200);

   AllocatedArea area1 = allocator_get(allocator, 25);
   AllocatedArea area2 = allocator_get(allocator, 25);
   AllocatedArea area3 = allocator_get(allocator, 1);

   assertInt(area1.size, 25);
   assertInt(area2.size, 25);
   assertInt(area3.size, 0);

   if(area1.address > area2.address){
      AllocatedArea temp = area1;
      area1 = area2;
      area2 = temp;
   }
   assertInt(area1.address, 0);
   assertInt(area2.address, 225);
}

TEST(size1, get100HintedHight_returnsHighest100){
   AllocatedArea area = allocator_getHinted(allocator, 100, AllocatorHintPreferHighAddresses);
   
   assertInt(area.size, 100);
   assertInt(area.address, size - 100);
}

TEST(size1, get200HintedHight_returnsHighest200){
   AllocatedArea area = allocator_getHinted(allocator, 200, AllocatorHintPreferHighAddresses);
   
   assertInt(area.size, 200);
   assertInt(area.address, size - 200);
}
TEST(size2, get2HintedHight_returnsHighest2){
   AllocatedArea area = allocator_getHinted(allocator, 2, AllocatorHintPreferHighAddresses);
   
   assertInt(area.size, 2);
   assertInt(area.address, size - 2);
}
TEST(size1, get100TwiceHintedHighes_returns2Highest){
   AllocatedArea area1 = allocator_getHinted(allocator, 100, AllocatorHintPreferHighAddresses);
   AllocatedArea area2 = allocator_getHinted(allocator, 100, AllocatorHintPreferHighAddresses);
   
   assertInt(area1.size, 100);
   assertInt(area1.address, size - 100);
   assertInt(area2.size, 100);
   assertInt(area2.address, size - 200);
}
TEST(size1, get100HintedLastWithMiddleReserved_returnsHighes){
   allocator_markAsReserved(allocator, size/2, 100);

   AllocatedArea area = allocator_getHinted(allocator, 100, AllocatorHintPreferHighAddresses);

   assertInt(area.size, 100);
   assertInt(area.address, size - 100);
}

TEST(size1, get1000HintedLast_WithLast1000NonContinuous_returnsAreaBefore){
   allocator_markAsReserved(allocator, size - 1000, 100);

   AllocatedArea area = allocator_getHinted(allocator, 1000, AllocatorHintPreferHighAddresses);

   assertInt(area.size, 1000);
   assertInt(area.address, size - 2000);
}


END_TESTS
