#include "testrunner.h"
#include "intlist.h"

static List *list;

TEST_GROUP_SETUP(dtg){
   list = list_newLinkedList(intlist_equals);
}

TEST_GROUP_TEARDOWN(dtg){
   intlist_freeList(list);
}

TESTS

TEST(dtg, listInitiallyEmpty){
   assertInt(intlist_length(list), 0);
}

TEST(dtg, emptyListDoesNotContain0x69){
   assertInt(intlist_contains(list, 0x69), false);
}

TEST(dtg, listAdd1Item_length1_containsItem){
   uintptr_t value = 0x69;
   intlist_add(list, value);

   assertInt(intlist_length(list), 1);
   assertInt(intlist_contains(list, value), true);
}

TEST(dtg, listAdd10Items_length10_contiansItems){
   for(int i = 0; i < 10; i++){
      intlist_add(list, i);
   }

   assertInt(intlist_length(list), 10);

   for(int i = 0; i < 10 && intlist_contains(list, i); i++);
}

TEST(dtg, listAdd1Item_getSameItem){
   uintptr_t value = 0x69;
   intlist_add(list, value);
   
   assertInt(intlist_get(list, 0), value);
}

TEST(dtg, listAdd10Item_getSameItems){
   for(int i = 0; i < 10; i++){
      intlist_add(list, i);
   }

   for(int i = 0; i < 10 && assertInt(intlist_get(list, i), i); i++);
}

TEST(dtg, listAddItem_thenRemoveAt0_doesNotContainItem){
   uintptr_t value = 0x69;
   intlist_add(list, value);

   bool success = intlist_removeAt(list, 0);

   assertInt(success, true);
   assertInt(intlist_length(list), 0);
   assertInt(intlist_contains(list, value), false);
}

TEST(dtg, listAddItem_thenRemoveItem_doesNotContainItem){
   uintptr_t value = 0x69;
   intlist_add(list, value);

   bool success = intlist_remove(list, value);

   assertInt(success, true);
   assertInt(intlist_length(list), 0);
   assertInt(intlist_contains(list, value), false);
}

TEST(dtg, listAdd3Items_removeAt0_doesNotContainItem_butContainsOther){
   for(int i = 0; i < 3; i++){
      intlist_add(list, i);
   }

   bool success = intlist_removeAt(list, 0);

   assertInt(success, true);
   assertInt(intlist_length(list), 2);
   assertInt(intlist_contains(list, 0), false);
   assertInt(intlist_contains(list, 1), true);
   assertInt(intlist_contains(list, 2), true);
}

TEST(dtg, listAdd3Items_removeAt1_doesNotContainItem_butContainsOther){
   for(int i = 0; i < 3; i++){
      intlist_add(list, i);
   }

   bool success = intlist_removeAt(list, 1);

   assertInt(success, true);
   assertInt(intlist_length(list), 2);
   assertInt(intlist_contains(list, 0), true);
   assertInt(intlist_contains(list, 1), false);
   assertInt(intlist_contains(list, 2), true);
}

TEST(dtg, listAdd3Items_removeAt2_doesNotContainItem_butContainsOther){
   for(int i = 0; i < 3; i++){
      intlist_add(list, i);
   }

   bool success = intlist_removeAt(list, 2);

   assertInt(success, true);
   assertInt(intlist_length(list), 2);
   assertInt(intlist_contains(list, 0), true);
   assertInt(intlist_contains(list, 1), true);
   assertInt(intlist_contains(list, 2), false);
}

TEST(dtg, listAdd3Items_removeLast_addItem_containsNewItem){
   for(int i = 0; i < 3; i++){
      intlist_add(list, i);
   }

   intlist_removeAt(list, 2);
   intlist_add(list, 0x69);

   assertInt(intlist_contains(list, 0), true);
   assertInt(intlist_contains(list, 1), true);
   assertInt(intlist_contains(list, 2), false);
   assertInt(intlist_contains(list, 0x69), true);
}

TEST(dtg, listAddItem_removeItem_addItem_containsNewItem){
   uintptr_t removeValue = 1, newValue = 0x69;
   intlist_add(list, removeValue);
   intlist_remove(list, removeValue);
   intlist_add(list, newValue);

   assertInt(intlist_contains(list, removeValue), false);
   assertInt(intlist_contains(list, newValue), true);
}

END_TESTS
