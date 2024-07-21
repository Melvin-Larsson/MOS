#include "stdlib.h"
#include "testrunner.h"
#include "intmap.h"

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

TEST_GROUP_SETUP(group){
   map = map_newBinaryMap(intmap_comparitor); 
}

TEST_GROUP_TEARDOWN(group){
   map->free(map, 0);
}

TESTS

TEST(group, addRightLeft){
   intmap_add(map, 5, 0);
   intmap_add(map, 4, 0);
   intmap_add(map, 6, 0);
   intmap_add(map, 10, 0);
   intmap_add(map, 9, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addRightRight){
   intmap_add(map, 5, 0);
   intmap_add(map, 4, 0);
   intmap_add(map, 6, 0);
   intmap_add(map, 10, 0);
   intmap_add(map, 11, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addLeftRight){
   intmap_add(map, 5, 0);
   intmap_add(map, 6, 0);
   intmap_add(map, 4, 0);
   intmap_add(map, 2, 0);
   intmap_add(map, 3, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addLeftLeft){
   intmap_add(map, 5, 0);
   intmap_add(map, 6, 0);
   intmap_add(map, 4, 0);
   intmap_add(map, 3, 0);
   intmap_add(map, 2, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addRightContains){
   int count = 10;
   for(int i = 0; i < count; i++){
      intmap_add(map, i, 0);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      assertIntNotEquals(intmap_contains(map, i), 0);
   }
}

TEST(group, addLeftContains){
   for(int i = 9; i >= 0; i--){
      intmap_add(map, i, 0);
   }
   
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertIntNotEquals(intmap_contains(map, i), 0);
   }
}

TEST(group, addLeftThenRightContains){
   intmap_add(map, 5, 0);
   intmap_add(map, 4, 0);
   intmap_add(map, 6, 0);
   intmap_add(map, 3, 0);
   intmap_add(map, 7, 0);
   intmap_add(map, 2, 0);
   intmap_add(map, 8, 0);
   intmap_add(map, 1, 0);
   intmap_add(map, 9, 0);
   intmap_add(map, 0, 0);

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertIntNotEquals(intmap_contains(map, i), 0);
   }
}

TEST(group, addRightGet){
   for(int i = 0; i < 10; i++){
      intmap_add(map, i, i);
   }


   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)intmap_get(map, i), i);
   }
}

TEST(group, addLeftGet){
   for(int i = 9; i >= 0; i--){
      intmap_add(map, i, i);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)intmap_get(map, i), i);
   }
}

TEST(group, addLeftThenRightContains){
   intmap_add(map, 5, 5);
   intmap_add(map, 4, 4);
   intmap_add(map, 6, 6);
   intmap_add(map, 3, 3);
   intmap_add(map, 7, 7);
   intmap_add(map, 2, 2);
   intmap_add(map, 8, 8);
   intmap_add(map, 1, 1);
   intmap_add(map, 9, 9);
   intmap_add(map, 0, 0);

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)intmap_get(map, i), i);
   }
}

TEST(group, addThenRemove){
   intmap_add(map, 0, 42);
   intmap_remove(map, 0);

   assertInt(intmap_contains(map, 0), 0);
   assertIntNotEquals(map_validate(map), 0);
}
TEST(group, add2RightThenRemoveInOrder){
   intmap_add(map, 0, 42);
   intmap_add(map, 1, 42);

   intmap_remove(map, 0);

   
   assertIntNotEquals(map_validate(map), 0);
   assertInt(intmap_contains(map, 0), 0);
   assertIntNotEquals(intmap_contains(map, 1), 0);

   intmap_remove(map, 1);

   assertIntNotEquals(map_validate(map), 0);
   assertInt(intmap_contains(map, 1), 0);
}

TEST(group, removeNotContains){
   int count = 10;
   for(int i = 0; i < count; i++){
      intmap_add(map, i, i);
   } 
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      intmap_remove(map, i);
      
      assertIntNotEquals(map_validate(map), 0);
      assertInt(intmap_contains(map, i), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(intmap_contains(map, j), 0);
      }
   }
}

TEST(group, insertRemoveRandomOrdered){
   int count = 13;
   int values[] = {62, 52, 85, 65, 3, 48, 46, 49, 60, 22, 73, 11, 44};
   int removeOrder[] = {11, 1, 6, 5, 12, 10, 3, 4, 0, 2, 9, 8, 7};

   for(int i = 0; i < count; i++){
      intmap_add(map, values[i], values[i]);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      intmap_remove(map, values[removeOrder[i]]);

      assertIntNotEquals(map_validate(map), 0);
      assertInt(intmap_contains(map, values[removeOrder[i]]), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(intmap_contains(map, values[removeOrder[j]]), 0);
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

      intmap_add(map, number, 0);
      int index = randomFreeIndex(removeNumbers, count);
      removeNumbers[index] = number;
   }
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      intmap_remove(map, removeNumbers[i]);
      assertInt(intmap_contains(map, removeNumbers[i]), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(intmap_contains(map, removeNumbers[j]), 0);
      }
   }
   assertIntNotEquals(map_validate(map), 0);
}

END_TESTS
