#include "kernel/allocator.h"
#include "stdlib.h"

typedef struct AllocatorList{
   struct AllocatorList *next;
   uintptr_t address;
   int size;
}AllocatorList;

Allocator* allocator_init(uintptr_t address, unsigned int size){
   Allocator *allocator = calloc(sizeof(Allocator));
   if(size == 0){
      return allocator;
   }
   AllocatorList *entry = malloc(sizeof(AllocatorList));
   *entry = (AllocatorList){
      .next = 0,
      .address = address,
      .size = size
   };

   AllocatorList *dummyEntry = malloc(sizeof(AllocatorList));
   *dummyEntry = (AllocatorList){
      .next = entry,
      .address = 0,
      .size = 0,
   };
   allocator->data = dummyEntry;
   return allocator;
}

void allocator_free(Allocator *allocator){
   AllocatorList *entry = allocator->data;

   while(entry){
      AllocatorList *next = entry->next;
      free(entry);
      entry = next;
   }

   free(allocator);
}

static void merge(Allocator *allocator){
   AllocatorList *prev = allocator->data;
   AllocatorList *list = prev->next;

   while(list){
      if(list->size <= 0){
         AllocatorList *next = list->next;
         free(list);
         prev->next = next;
         list = next;
         continue;
      } 
      else if(list->next && list->address + list->size >= list->next->address){
         AllocatorList *next = list->next;
         int newSize = next->address + next->size - list->address;
         next->address = list->address;
         next->size = newSize;
         free(list);
         prev->next = next;
         list = next; 
         continue;
      }
      prev = list;
      list = list->next;
   }
}

AllocatedArea allocator_get(Allocator *allocator, int size){
   AllocatorList *dummy = allocator->data;
   AllocatorList *list = dummy->next;

   while(list && list->size < size){
      list = list->next;
   }
   if(!list){
      return (AllocatedArea){0,0};
   }

   uintptr_t resultAddress;
   resultAddress = list->address;
   list->address += size;
   list->size -= size;

   return (AllocatedArea){
      .address = resultAddress,
      .size = size,
   };
}

void allocator_release(Allocator *allocator, uintptr_t address, int size){
   AllocatorList *list = allocator->data;

   while(list->next && address > list->next->address){
      list = list->next;
   }
   AllocatorList *newEntry = malloc(sizeof(AllocatorList));
   *newEntry = (AllocatorList){
      .next = list->next,
      .address = address,
      .size = size,
   };
   list->next = newEntry;
   merge(allocator);
   return; 
}

static int overlaps(uintptr_t address1, int size1, uintptr_t address2, int size2){
   return address1 < address2 + size2 && address1 + size1 > address2;
}
static AllocatorList *split(AllocatorList *area, int lowerSize){
   AllocatorList *newArea = malloc(sizeof(AllocatorList));
   *newArea = (AllocatorList){
      .next = area->next,
      .address = area->address + lowerSize,
      .size = area->size - lowerSize,
   };
   area->size = lowerSize;
   area->next = newArea;
   return area;
}
static void remove(AllocatorList *toRemove, AllocatorList *prev){
   prev->next = toRemove->next;
   free(toRemove);
}

void allocator_markAsReserved(Allocator *allocator, uintptr_t address, int size){
   AllocatorList *prev = allocator->data;
   AllocatorList *list = prev->next;

   while(list){
      if(overlaps(address, size, list->address, list->size)){
         if(list->address < address && list->address + list->size > address + size){
            int sizeBefore = address - list->address;
            list = split(list, sizeBefore + size);
            list = split(list, sizeBefore);
            remove(list->next, list);
         }
         else if(address <= list->address){
            int diff = address + size - list->address;
            list->address += diff;
            list->size -= diff;
         }
         else if(address + size >= list->address + list->size){
            int diff = list->address + list->size - address;
            list->size -= diff;
         }
      }
      list = list->next;
   }
}
