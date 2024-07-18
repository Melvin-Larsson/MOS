#include "stdlib.h"
#include "testrunner.h"
#include "map.h"

static Map *map;


static int randomNumber(){
   static unsigned long int next = 1;
   next = next * 1103515245 + 12345;
   return (unsigned int) (next / 65536) % 32768;
}

int randomFreeIndex(int *array, int size){
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
int contains(int *array, int size, int value){
   while(size--){
      if(*array++ == value){
         return 1;
      }
   }
   return 0;
}

TEST_GROUP_SETUP(group){
   map = map_newBinaryMap(); 
}

TEST_GROUP_TEARDOWN(group){
   map->free(map, 0);
}

TESTS

TEST(group, addRightLeft){
   map_add(map, 5, 0);
   map_add(map, 4, 0);
   map_add(map, 6, 0);
   map_add(map, 10, 0);
   map_add(map, 9, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addRightRight){
   map_add(map, 5, 0);
   map_add(map, 4, 0);
   map_add(map, 6, 0);
   map_add(map, 10, 0);
   map_add(map, 11, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addLeftRight){
   map_add(map, 5, 0);
   map_add(map, 6, 0);
   map_add(map, 4, 0);
   map_add(map, 2, 0);
   map_add(map, 3, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addLeftLeft){
   map_add(map, 5, 0);
   map_add(map, 6, 0);
   map_add(map, 4, 0);
   map_add(map, 3, 0);
   map_add(map, 2, 0);

   assertIntNotEquals(map_validate(map), 0);
}

TEST(group, addRightContains){
   int count = 10;
   for(int i = 0; i < count; i++){
      map_add(map, i, 0);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      assertIntNotEquals(map_contains(map, i), 0);
   }
}

TEST(group, addLeftContains){
   for(int i = 9; i >= 0; i--){
      map_add(map, i, 0);
   }
   
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertIntNotEquals(map_contains(map, i), 0);
   }
}

TEST(group, addLeftThenRightContains){
   map_add(map, 5, 0);
   map_add(map, 4, 0);
   map_add(map, 6, 0);
   map_add(map, 3, 0);
   map_add(map, 7, 0);
   map_add(map, 2, 0);
   map_add(map, 8, 0);
   map_add(map, 1, 0);
   map_add(map, 9, 0);
   map_add(map, 0, 0);

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertIntNotEquals(map_contains(map, i), 0);
   }
}

TEST(group, addRightGet){
   for(int i = 0; i < 10; i++){
      map_add(map, i, (void*)i);
   }


   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)map_get(map, i), i);
   }
}

TEST(group, addLeftGet){
   for(int i = 9; i >= 0; i--){
      map_add(map, i, (void*)i);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)map_get(map, i), i);
   }
}

TEST(group, addLeftThenRightContains){
   map_add(map, 5,(void*)5);
   map_add(map, 4,(void*)4);
   map_add(map, 6,(void*)6);
   map_add(map, 3,(void*)3);
   map_add(map, 7,(void*)7);
   map_add(map, 2,(void*)2);
   map_add(map, 8,(void*)8);
   map_add(map, 1,(void*)1);
   map_add(map, 9,(void*)9);
   map_add(map, 0,(void*)0);

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < 10; i++){
      assertInt((int)map_get(map, i), i);
   }
}

TEST(group, addThenRemove){
   map_add(map, 0, (void*)42);
   map_remove(map, 0);

   assertInt(map_contains(map, 0), 0);
   assertIntNotEquals(map_validate(map), 0);
}
TEST(group, add2RightThenRemoveInOrder){
   map_add(map, 0, (void*)42);
   map_add(map, 1, (void*)42);

   map_remove(map, 0);

   
   assertIntNotEquals(map_validate(map), 0);
   assertInt(map_contains(map, 0), 0);
   assertIntNotEquals(map_contains(map, 1), 0);

   map_remove(map, 1);

   assertIntNotEquals(map_validate(map), 0);
   assertInt(map_contains(map, 1), 0);
}

TEST(group, removeNotContains){
   int count = 10;
   for(int i = 0; i < count; i++){
      map_add(map, i, (void*)i);
   } 
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      map_remove(map, i);
      
      assertIntNotEquals(map_validate(map), 0);
      assertInt(map_contains(map, i), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(map_contains(map, j), 0);
      }
   }
}

TEST(group, insertRemoveRandomOrdered){
   int count = 13;
   int values[] = {62, 52, 85, 65, 3, 48, 46, 49, 60, 22, 73, 11, 44};
   int removeOrder[] = {11, 1, 6, 5, 12, 10, 3, 4, 0, 2, 9, 8, 7};

   for(int i = 0; i < count; i++){
      map_add(map, values[i], (void*)values[i]);
   }

   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      map_remove(map, values[removeOrder[i]]);

      assertIntNotEquals(map_validate(map), 0);
      assertInt(map_contains(map, values[removeOrder[i]]), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(map_contains(map, values[removeOrder[j]]), 0);
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

      map_add(map, number, 0);
      int index = randomFreeIndex(removeNumbers, count);
      removeNumbers[index] = number;
   }
   assertIntNotEquals(map_validate(map), 0);

   for(int i = 0; i < count; i++){
      map_remove(map, removeNumbers[i]);
      assertInt(map_contains(map, removeNumbers[i]), 0);

      for(int j = i + 1; j < count; j++){
         assertIntNotEquals(map_contains(map, removeNumbers[j]), 0);
      }
   }
   assertIntNotEquals(map_validate(map), 0);
}

END_TESTS
