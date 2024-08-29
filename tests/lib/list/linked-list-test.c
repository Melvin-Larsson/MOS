#include "testrunner.h"
#include "collection/intlist.h"
#include "collection/int-iterator.h"

static List *list;
static Iterator *iterator;

TEST_GROUP_SETUP(dtg){
   list = list_newLinkedList(intlist_equals);
   iterator = intlist_createIterator(list);
}

TEST_GROUP_TEARDOWN(dtg){
   intlist_freeList(list);
   intIterator_free(iterator);
}

TEST_GROUP_SETUP(10ItemList){
   list = list_newLinkedList(intlist_equals);
   for(int i = 0; i < 10; i++){
      intlist_add(list, i);
   }
   iterator = intlist_createIterator(list);
}

TEST_GROUP_TEARDOWN(10ItemList){
   intlist_freeList(list);
   intIterator_free(iterator);
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

TEST(dtg, iterator_hasNextOnEmpty_returnsFalse){
   assertInt(intIterator_hasNext(iterator), false);
}

TEST(10ItemList, iterator_hasNextOnNonEmpty_returnsTrue){
   assertInt(intIterator_hasNext(iterator), true);
}

TEST(10ItemList, iterator_advanceThenGetReturnsFirst){
   intIterator_advance(iterator);
   assertInt(intIterator_get(iterator), 0);
}

TEST(10ItemList, iteratorAdvance_getReturnsCorrect){
   for(int i = 0; i < 10; i++){
      intIterator_advance(iterator);
      if(!assertInt(intIterator_get(iterator), i)){
         break;
      }
   }
}

TEST(10ItemList, iteratorAddAfterAt0_listContainsValue_newLength){
   uintptr_t data = 0x69;
   intIterator_addAfter(iterator, data);

   assertInt(intlist_contains(list, data), true);
   assertInt(intlist_get(list, 0), data);
   assertInt(intlist_length(list), 11);
}

TEST(10ItemList, iteratorAddAfterAt1_listContainsValue_newLength){
   uintptr_t data = 0x69;
   intIterator_advance(iterator);
   intIterator_addAfter(iterator, data);

   assertInt(intlist_contains(list, data), true);
   assertInt(intlist_get(list, 1), data);
   assertInt(intlist_length(list), 11);
}

TEST(10ItemList, iteratorAddAfterAtLast_listContainsValue_newLength){
   uintptr_t data = 0x69;
   for(int i = 0; i < 10; i++){
      intIterator_advance(iterator);
   }
   intIterator_addAfter(iterator, data);

   assertInt(intlist_contains(list, data), true);
   assertInt(intlist_get(list, 10), data);
   assertInt(intlist_length(list), 11);
}

TEST(10ItemList, iteratorRemoveFirst_valueNoLongerInList_newLength){
   intIterator_advance(iterator);

   bool success = intIterator_remove(iterator);

   assertInt(success, true);
   assertInt(intlist_contains(list, 0), false);
   assertInt(intlist_get(list, 0), 1);
   assertInt(intlist_length(list), 9);
}

TEST(10ItemList, iteratorRemove2nd_valueNoLongerInList_newLength){
   intIterator_advance(iterator);
   intIterator_advance(iterator);

   bool success = intIterator_remove(iterator);

   assertInt(success, true);
   assertInt(intlist_contains(list, 1), false);
   assertInt(intlist_length(list), 9);
}

TEST(10ItemList, iteratorRemoveLast_valueNoLongerInList_newLength){
   for(int i = 0; i < 10; i++){
      intIterator_advance(iterator);
   }

   bool success = intIterator_remove(iterator);

   assertInt(success, true);
   assertInt(intlist_contains(list, 9), false);
   assertInt(intlist_length(list), 9);
}

TEST(dtg, iteratorAdd_thenAddNormally_length2_containsBoth){
   uintptr_t data1 = 0x69, data2 = 0x70;
   intIterator_addAfter(iterator, data1);
   intlist_add(list, data2);

   assertInt(intlist_contains(list, data1), true);
   assertInt(intlist_contains(list, data2), true);
   assertInt(intlist_length(list), 2);
}

TEST(10ItemList, iteratorAddLast_thenAddNormally_newLength_containsBoth){
   uintptr_t data1 = 0x69, data2 = 0x70;
   for(int i = 0; i < 10; i++){
      intIterator_advance(iterator);
   }
   intIterator_addAfter(iterator, data1);
   intlist_add(list, data2);

   assertInt(intlist_contains(list, data1), true);
   assertInt(intlist_contains(list, data2), true);
   assertInt(intlist_length(list), 12);
}

TEST(10ItemList, iteratorRemoveLastItem_AddNormally_length10_containsNew){
   uintptr_t data = 0x70;

   for(int i = 0; i < 10; i++){
      intIterator_advance(iterator);
   }
   intIterator_remove(iterator);

   intlist_add(list, data);

   assertInt(intlist_contains(list, data), true);
   assertInt(intlist_length(list), 10);
}

TEST(10ItemList, iteratorRemoveAll_AddNormally_length1_containsNew){
   uintptr_t data = 0x70;

   intIterator_advance(iterator);
   for(int i = 0; i < 10; i++){
      assertInt(intIterator_remove(iterator), true);
   }

   intlist_add(list, data);

   assertInt(intlist_contains(list, data), true);
   assertInt(intlist_length(list), 1);
}

TEST(10ItemList, modifyWhileIterating_allFunctionsReturnFalse){
   intlist_add(list, 1);

   assertInt(intIterator_hasNext(iterator), false);
   assertInt(intIterator_advance(iterator), false);
   assertInt(intIterator_remove(iterator), false);
   assertInt(intIterator_addAfter(iterator, 0), false);
   assertInt(intIterator_get(iterator), false);
}

TEST(10ItemList, iteratorRemoveBeforeAdvance_successFalse){
   assertInt(intIterator_remove(iterator), false);
   assertInt(intlist_length(list), 10);
}

TEST(dtg, iteratorGetBeforeAdvance_returns0){
   intlist_add(list, 0x69);
   Iterator *it = intlist_createIterator(list);
   assertInt(intIterator_get(it), 0);
   it->free(it);
}

TEST(10ItemList, iteratorAddAt_containsNew){
   uintptr_t data = 0x69;
   intIterator_advance(iterator);
   intIterator_addAt(iterator, data);

   assertInt(intlist_length(list), 11);
   assertInt(intlist_get(list, 0), data);
}

TEST(10ItemList, iteratorAddAtLast_containsNew){
   uintptr_t data = 0x69;
   for(int i = 0; i < 10; i++){
      intIterator_advance(iterator);
   }
   intIterator_addAt(iterator, data);

   assertInt(intlist_length(list), 11);
   assertInt(intlist_get(list, 9), data);
}

TEST(10ItemList, iteratorAddAtLast_beforeAdvance_successFalse){
   bool success = intIterator_addAt(iterator, 0x69);

   assertInt(success,  false);
   assertInt(intlist_length(list), 10);
}

END_TESTS
