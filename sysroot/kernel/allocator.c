#include "kernel/allocator.h"
#include "kernel/memory.h"
#include "kernel/memory-allocator.h"
#include "stdlib.h"
#include "stdio.h"
#include "utils/assert.h"

typedef struct AllocatorList{
   struct AllocatorList *next;
   uintptr_t address;
   int size;
}AllocatorList;

typedef struct{
   AllocatorList *list;
   Memory *memory;
}AllocatorData;

static void *allocateMemory(AllocatorData *allocator, size_t size);
static void freeMemory(AllocatorData *allocator, void *data);

Allocator *allocator_initManaged(Memory *memory, uintptr_t address, size_t size){
   assert(memory != 0);

   Allocator *allocator = memory_malloc(memory, sizeof(Allocator));
   AllocatorData *data = memory_malloc(memory, sizeof(AllocatorData));
   AllocatorList *dummyEntry = memory_malloc(memory, sizeof(AllocatorList));

   if(!allocator || !data || !dummyEntry){
      return 0;
   }

   AllocatorList *entry = 0;
   if(size != 0){
      entry = memory_malloc(memory, sizeof(AllocatorList));
      if(!entry){
         return 0;
      }
      *entry = (AllocatorList){
         .next = 0,
         .address = address,
         .size = size
      };
   }
   *dummyEntry = (AllocatorList){
      .next = entry,
      .address = 0,
      .size = 0,
   };

   *data = (AllocatorData){
      .list = dummyEntry,
      .memory = memory
   };
   allocator->data = data;

   return allocator;
}

Allocator* allocator_init(uintptr_t address, unsigned int size){
   Allocator *allocator = kmalloc(sizeof(Allocator));
   AllocatorData *data = kmalloc(sizeof(AllocatorData));
   AllocatorList *dummyEntry = kmalloc(sizeof(AllocatorList));

   if(!allocator || !data || !dummyEntry){
      goto on_error;
   }
   memset(allocator, 0, sizeof(Allocator));

   AllocatorList *entry = 0;
   if(size != 0){
      entry = kmalloc(sizeof(AllocatorList));
      if(!entry){
         goto on_error;
      }
      *entry = (AllocatorList){
         .next = 0,
         .address = address,
         .size = size
      };
   }
   *dummyEntry = (AllocatorList){
      .next = entry,
      .address = 0,
      .size = 0,
   };
   *data = (AllocatorData){
      .list = dummyEntry,
      .memory = 0
   };
   allocator->data = data;

   return allocator;

on_error:
      kfree(allocator);
      kfree(data);
      kfree(dummyEntry);
      return 0;
}

void allocator_free(Allocator *allocator){
   AllocatorData *data = allocator->data;
   if(data->memory){
      return;
   }

   AllocatorList *entry = data->list;

   while(entry){
      AllocatorList *next = entry->next;
      kfree(entry);
      entry = next;
   }

   kfree(data);
   kfree(allocator);
}

static void merge(Allocator *allocator){
   AllocatorData *data = allocator->data;
   AllocatorList *prev = data->list;
   AllocatorList *list = prev->next;

   while(list){
      if(list->size <= 0){
         AllocatorList *next = list->next;
         freeMemory(data, list);
         prev->next = next;
         list = next;
         continue;
      } 
      else if(list->next && list->address + list->size >= list->next->address){
         AllocatorList *next = list->next;
         int newSize = next->address + next->size - list->address;
         next->address = list->address;
         next->size = newSize;
         freeMemory(data, list);
         prev->next = next;
         list = next; 
         continue;
      }
      prev = list;
      list = list->next;
   }
}

static AllocatedArea getHigh(AllocatorList *list, unsigned int size){
   if(!list){
      return (AllocatedArea){0,0};
   }
   AllocatedArea area = getHigh(list->next, size);

   if(area.size == size){
      return area;
   }

   if(list->size > 0 && (unsigned int)list->size >= size){
      AllocatedArea result = {list->address + list->size - size, size};
      list->size -= size;
      return result;
   }
   return (AllocatedArea){0,0};
}

AllocatedArea allocator_getHinted(Allocator *allocator, int size, AllocatorHint hint){
   AllocatorData *data = allocator->data;
   if(hint == AllocatorHintPreferHighAddresses){
      return getHigh(data->list, size);
   }
   return allocator_get(allocator, size);
}

AllocatedArea allocator_get(Allocator *allocator, int size){
   AllocatorData *data = allocator->data;
   AllocatorList *dummy = data->list;
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

bool allocator_release(Allocator *allocator, uintptr_t address, int size){
   AllocatorData *data = allocator->data;
   AllocatorList *list = data->list;

   while(list->next && address > list->next->address){
      list = list->next;
   }

   AllocatorList *newEntry = allocateMemory(data, sizeof(AllocatorList));
   if(!newEntry){
      return false;
   }
   *newEntry = (AllocatorList){
      .next = list->next,
      .address = address,
      .size = size,
   };
   list->next = newEntry;
   merge(allocator);

   return true;
}

static int overlaps(uintptr_t address1, int size1, uintptr_t address2, int size2){
   return address1 < address2 + size2 && address1 + size1 > address2;
}
static AllocatorList *split(AllocatorData *allocator, AllocatorList *area, int lowerSize){
   AllocatorList *newArea = allocateMemory(allocator, sizeof(AllocatorList));
   if(!newArea){
      return 0;
   }
   *newArea = (AllocatorList){
      .next = area->next,
      .address = area->address + lowerSize,
      .size = area->size - lowerSize,
   };
   area->size = lowerSize;
   area->next = newArea;
   return area;
}
static void remove(AllocatorData *allocator, AllocatorList *toRemove, AllocatorList *prev){
   prev->next = toRemove->next;
   freeMemory(allocator, toRemove);
}

bool allocator_markAsReserved(Allocator *allocator, uintptr_t address, int size){
   AllocatorData *data = allocator->data;
   AllocatorList *prev = data->list;
   AllocatorList *list = prev->next;

   while(list){
      if(overlaps(address, size, list->address, list->size)){
         if(list->address < address && list->address + list->size > address + size){
            int sizeBefore = address - list->address;
            list = split(data, list, sizeBefore + size);
            if(!list){
               return false;
            }
            list = split(data, list, sizeBefore);
            if(!list){
               return false;
            }
            remove(data, list->next, list);
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

   return true;
}

static void *allocateMemory(AllocatorData *allocator, size_t size){
   if(allocator->memory){
      return memory_malloc(allocator->memory, size);
   }
   return kmalloc(size);
}

static void freeMemory(AllocatorData *allocator, void *data){
   if(allocator->memory){
      memory_free(allocator->memory, data);
   }
   kfree(data);
}

