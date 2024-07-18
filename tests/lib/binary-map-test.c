#include "stdlib.h"
#include "testrunner.h"
#include "map.h"

static Map *map;


static int randomNumber(){
   static unsigned long int next = 1;
   next = next * 1103515245 + 12345;
   return (unsigned int) (next / 65536) % 32768;
}

static int randomFreeIndex(int *array, int size){
   int index = randomNumber();

   for(int i = 0; index > 0; i = (i + 1) % size){
      if(!array[i]){
         index--; 
         if(index == 0){
            return i;
         }
      }
   }
   return 0;
}
static int contains(int *array, int size, int value){
   while(size--){
      if(*array++ == value){
         return 1;
      }
   }
   return 0;
}

static int addIntToMap(const Map *map, uintptr_t key, uintptr_t value){
   return map_add(map, (void *)key, (void *)value);
}

static int mapContainsInt(const Map *map, uintptr_t key){
   return map_contains(map, (void *)key);
}
static int mapRemoveInt(const Map *map, uintptr_t key){
   return map_remove(map, (void *)key);
}
static uintptr_t mapGetInt(const Map *map, uintptr_t key){
   return (uintptr_t)map_get(map, (void *)key);
}

TEST_GROUP_SETUP(group){
   map = map_newBinaryMap(map_ptrComparitor); 
}

TEST_GROUP_TEARDOWN(group){
   map->free(map, 0);
}

TESTS

TEST(group, addRightLeft){
   addIntToMap(map, 5, 0);
   addIntToMap(map, 4, 0);
   addIntToMap(map, 6, 0);
   addIntToMap(map, 10, 0);
   addIntToMap(map, 9, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addRightRight){
   addIntToMap(map, 5, 0);
   addIntToMap(map, 4, 0);
   addIntToMap(map, 6, 0);
   addIntToMap(map, 10, 0);
   addIntToMap(map, 11, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addLeftRight){
   addIntToMap(map, 5, 0);
   addIntToMap(map, 6, 0);
   addIntToMap(map, 4, 0);
   addIntToMap(map, 2, 0);
   addIntToMap(map, 3, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addLeftLeft){
   addIntToMap(map, 5, 0);
   addIntToMap(map, 6, 0);
   addIntToMap(map, 4, 0);
   addIntToMap(map, 3, 0);
   addIntToMap(map, 2, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addRightContains){
   int count = 10;
   for(int i = 0; i < count; i++){
      addIntToMap(map, i, 0);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      assertIntNotEquals(mapContainsInt(map, i), 0);
   }
}

TEST(group, addLeftContains){
   for(int i = 9; i >= 0; i--){
      addIntToMap(map, i, 0);
   }
   
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertIntNotEquals(mapContainsInt(map, i), 0);
   }
}

TEST(group, addLeftThenRightContains){
   addIntToMap(map, 5, 0);
   addIntToMap(map, 4, 0);
   addIntToMap(map, 6, 0);
   addIntToMap(map, 3, 0);
   addIntToMap(map, 7, 0);
   addIntToMap(map, 2, 0);
   addIntToMap(map, 8, 0);
   addIntToMap(map, 1, 0);
   addIntToMap(map, 9, 0);
   addIntToMap(map, 0, 0);

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertIntNotEquals(mapContainsInt(map, i), 0);
   }
}

TEST(group, addRightGet){
   for(int i = 0; i < 10; i++){
      addIntToMap(map, i, i);
   }


   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)mapGetInt(map, i), i);
   }
}

TEST(group, addLeftGet){
   for(int i = 9; i >= 0; i--){
      addIntToMap(map, i, i);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)mapGetInt(map, i), i);
   }
}

TEST(group, addLeftThenRightContains){
   addIntToMap(map, 5, 5);
   addIntToMap(map, 4, 4);
   addIntToMap(map, 6, 6);
   addIntToMap(map, 3, 3);
   addIntToMap(map, 7, 7);
   addIntToMap(map, 2, 2);
   addIntToMap(map, 8, 8);
   addIntToMap(map, 1, 1);
   addIntToMap(map, 9, 9);
   addIntToMap(map, 0, 0);

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)mapGetInt(map, i), i);
   }
}

TEST(group, addThenRemove){
   addIntToMap(map, 0, 42);
   mapRemoveInt(map, 0);

   assertInt(mapContainsInt(map, 0), 0);
   assertIntNotEquals(map_validate(map), 0);
}
TEST(group, add2RightThenRemoveInOrder){
   addIntToMap(map, 0, 42);
   addIntToMap(map, 1, 42);

   mapRemoveInt(map, 0);

   
   assertIntNotEquals(map_validate(map), 0);
   assertInt(mapContainsInt(map, 0), 0);
   assertIntNotEquals(mapContainsInt(map, 1), 0);

   mapRemoveInt(map, 1);

   assertIntNotEquals(map_validate(map), 0);
   assertInt(mapContainsInt(map, 1), 0);
}

TEST(group, removeNotContains){
   int count = 10;
   for(int i = 0; i < count; i++){
      addIntToMap(map, i, i);
   } 
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      mapRemoveInt(map, i);
      
      assertIntNotEquals(map_validate(map), 0);
      assertInt(mapContainsInt(map, i), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(mapContainsInt(map, j), 0);
      }
   }
}

TEST(group, insertRemoveRandomOrdered){
   int count = 13;
   int values[] = {62, 52, 85, 65, 3, 48, 46, 49, 60, 22, 73, 11, 44};
   int removeOrder[] = {11, 1, 6, 5, 12, 10, 3, 4, 0, 2, 9, 8, 7};

   for(int i = 0; i < count; i++){
      addIntToMap(map, values[i], values[i]);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      mapRemoveInt(map, values[removeOrder[i]]);

      assertIntNotEquals(map_validate(map), 0);
      assertInt(mapContainsInt(map, values[removeOrder[i]]), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(mapContainsInt(map, values[removeOrder[j]]), 0);
      }
   }
}

TEST(group, randomNumbers){
   int count = 1000;
   int removeNumbers[count];
   memset(removeNumbers, 0, sizeof(removeNumbers));

   for(int i = 0; i < count; i++){
      int number = randomNumber();

      while(contains(removeNumbers, count, number)){
         number = randomNumber();
      }

      addIntToMap(map, number, 0);
      int index = randomFreeIndex(removeNumbers, count);
      removeNumbers[index] = number;
   }
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      mapRemoveInt(map, removeNumbers[i]);
      assertInt(mapContainsInt(map, removeNumbers[i]), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(mapContainsInt(map, removeNumbers[j]), 0);
      }
   }
   assertIntNotEquals(map_validate(map), 0);
}

END_TESTS
