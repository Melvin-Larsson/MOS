#include "kernel/physpage.h"

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

typedef enum{
   Memory = 1,
   Reserved = 2,
}AddressRangeType;

typedef struct PageStack{
   struct PageStack *next;
   uint64_t page;
   AddressRangeType pageCount4KB : 32;
}PageStack;

typedef struct{
   uint64_t address;
   uint64_t length;
   uint32_t type;
}AddressRange;

static PageStack *stack;

void physpage_init(){
   volatile uint32_t *base = ((volatile uint32_t *)0x500);
   uint32_t length = *base / sizeof(AddressRange);

   AddressRange *addressRangeTable = (AddressRange*)(base + 1);

   stack = 0;

   for(uint32_t i = 0; i < length; i++){
      if(addressRangeTable[i].type != Memory){
         continue;
      }
      PageStack *top = malloc(sizeof(PageStack));
      *top = (PageStack){
         .page = (addressRangeTable[i].address + 4 * 1024 - 1) / (4 * 1024),
         .pageCount4KB = addressRangeTable[i].length / (4 * 1024),
         .next = stack
      };
      stack = top;
   }
}

uint64_t physpage_getPage4KB(){
   if(!stack){
      return 0;
   }
   if(stack->pageCount4KB){
      stack->page++;
      stack->pageCount4KB--;
      return stack->page - 1;
   }
   else{
      PageStack *last = stack;
      stack = stack->next;
      free(last);
      return physpage_getPage4KB();
   }
}
static void splitPage(PageStack *toSplit, uint64_t sizeOfFirst4KB){
   assert(sizeOfFirst4KB < toSplit->pageCount4KB);

   PageStack *sndHalf = malloc(sizeof(PageStack));
   *sndHalf = (PageStack){
      .page = toSplit->page + sizeOfFirst4KB,
      .pageCount4KB = toSplit->pageCount4KB - sizeOfFirst4KB,
      .next = toSplit->next
   };

   toSplit->pageCount4KB = sizeOfFirst4KB;
   toSplit->next = sndHalf;
}
static void deletePage(PageStack *last, PageStack *toDelete){
   last->next = toDelete->next;   
   free(toDelete);
}

uint64_t physpage_getPage4MB(){
   PageStack *last = 0;
   PageStack *curr = stack;

   while(curr){
      PageStack *next = curr->next;

      uint64_t offsetToAligned = 1024 - curr->page % 1024;
      offsetToAligned = offsetToAligned == 1024 ? 0 : offsetToAligned;

      if(curr->pageCount4KB - offsetToAligned >= 1024){
         if(offsetToAligned == 0){
            curr->page += 1024;
            curr->pageCount4KB -= 1024;
            return (curr->page - 1024) / 1024;
         }else{
            splitPage(curr, offsetToAligned);
            uint64_t page = curr->next->page;
            deletePage(curr, curr->next);
            return page / 1024;
         }
      }
      else if(curr->pageCount4KB == 0){
         if(last){
            last->next = next;
         }else{
            stack = next;
         }
         free(curr);
      }

      curr = next;
   }

   return 0;
}
static void releasePage(uint64_t page, uint32_t countOf4KB){
   PageStack *top = malloc(sizeof(PageStack));
   *top = (PageStack){
      .page = page,
      .pageCount4KB = countOf4KB,
      .next = stack,
   };
   stack = top;

}
void physpage_releasePage4KB(uint64_t page){
   releasePage(page, 1);
}
void physpage_releasePage4MB(uint64_t page){
   releasePage(page * 1024, 1024);
}

static int overlaps(uint64_t start1, uint32_t length1, uint64_t start2, uint32_t length2){
   return start1 <= start2 + length2 && start1 + length1 >= start2;
}

void physpage_markPagesAsUsed4MB(uint64_t page, uint32_t count){
   physpage_markPagesAsUsed4KB(page * 1024, count * 1024);
}
void physpage_markPagesAsUsed4KB(uint64_t page, uint32_t count){
   PageStack *ptr = stack; 
   while(ptr){

      if(!overlaps(page, count, ptr->page, ptr->pageCount4KB)){
         ptr = ptr->next;
         continue;
      }

      if(page <= ptr->page && page + count >= ptr->page + ptr->pageCount4KB){
         ptr->pageCount4KB = 0;
      }
      else if(page <= ptr->page){
         uint32_t removeCount = page + count - ptr->page;
         ptr->page += removeCount;
         ptr->pageCount4KB -= removeCount;
      }
      else if(page + count >= ptr->page + ptr->pageCount4KB){
         uint32_t removeCount = ptr->page + ptr->pageCount4KB - page;
         ptr->pageCount4KB -= removeCount;
      }
      else{
         uint64_t sizeBefore = page - ptr->page;

         splitPage(ptr, sizeBefore + count);
         splitPage(ptr, sizeBefore);
         deletePage(ptr, ptr->next);
      }

      ptr = ptr->next;
   }
}
