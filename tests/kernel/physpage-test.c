#include "testrunner.h"
#include "physpage.c"
#include "stdlib.h"

TEST_GROUP_SETUP(consecutive){
   stack = malloc(sizeof(PageStack));
   *stack = (PageStack){
      .page = 0,
      .pageCount4KB = 1024 * 1024,
      .next = 0
   };
}
TEST_GROUP_TEARDOWN(consecutive){
   while(stack){
      PageStack *next = stack->next;
      free(stack);
      stack = next;
   }
}

TEST_GROUP_SETUP(fourthsMissing){
   for(int i = 1023; i >= 0; i--){
      PageStack *ptr = malloc(sizeof(PageStack));
      *ptr = (PageStack){
         .page = i * 4,
         .pageCount4KB = 3,
         .next = stack
      };
      stack = ptr;
   }
}
TEST_GROUP_TEARDOWN(fourthsMissing){
   while(stack){
      PageStack *next = stack->next;
      free(stack);
      stack = next;
   }
}

TESTS

TEST(consecutive, getpage4KBOnce){
   assertInt(physpage_getPage4KB(), 0);
}

TEST(fourthsMissing, getPage4KB3Times){
   assertInt(physpage_getPage4KB(), 0);
   assertInt(physpage_getPage4KB(), 1);
   assertInt(physpage_getPage4KB(), 2);
   assertInt(physpage_getPage4KB(), 4);
}

TEST(fourthsMissing, markPageAsUsed_markFullEntry){
   physpage_markPagesAsUsed4KB(0, 3);
   assertInt(physpage_getPage4KB(), 4);
}
TEST(fourthsMissing, markPageAsUsed_markPartialEntry){
   physpage_markPagesAsUsed4KB(1, 1);
   assertInt(physpage_getPage4KB(), 0);
   assertInt(physpage_getPage4KB(), 2);
}
TEST(fourthsMissing, markPageAsUsed_multipleEntries){
   //f f u u u u u u u u f u 
   physpage_markPagesAsUsed4KB(2, 8);
   assertInt(physpage_getPage4KB(), 0);
   assertInt(physpage_getPage4KB(), 1);
   assertInt(physpage_getPage4KB(), 10);
}

TEST(consecutive, getPage4KB_2To102ndPageUsed){
   physpage_markPagesAsUsed4KB(1, 100);
   assertInt(physpage_getPage4KB(), 0);
   assertInt(physpage_getPage4KB(), 101);
}

TEST(consecutive, getPage4KB_getRelaseGet){
   uint32_t page1 = physpage_getPage4KB();
   physpage_releasePage4KB(page1);
   assertInt(physpage_getPage4KB(), page1);
}

TEST(consecutive, getPage4MBOnce){
   assertInt(physpage_getPage4MB(), 0);
}
TEST(consecutive, getPage4MB100Times){
   for(int i = 0; i < 100; i++){
      uint32_t page = physpage_getPage4MB();
      assertInt(page, i);
   }
}
TEST(consecutive, getPage4MB_2ndPageUsed){
   physpage_markPagesAsUsed4MB(1, 1);
   assertInt(physpage_getPage4MB(), 0);
   assertInt(physpage_getPage4MB(), 2);
}

TEST(consecutive, getPage4MB_2To102ndPageUsed){
   physpage_markPagesAsUsed4MB(1, 100);
   assertInt(physpage_getPage4MB(), 0);
   assertInt(physpage_getPage4MB(), 101);
}
TEST(consecutive, getPage4MB_getRelaseGet){
   uint32_t page1 = physpage_getPage4MB();
   physpage_releasePage4MB(page1);
   assertInt(physpage_getPage4MB(), page1);
}

TEST(consecutive, getPage4KB_thenGetPage4MB){
   physpage_getPage4KB();
   assertInt(physpage_getPage4MB(), 1);
}
TEST(consecutive, getPage4MB_10thAnd1030th4KBMarkedAsUsed){
   physpage_markPagesAsUsed4KB(10, 1);
   physpage_markPagesAsUsed4KB(1030, 1);
   assertInt(physpage_getPage4MB(), 2);
}


END_TESTS
