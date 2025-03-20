#include "kernel/memory.h"
#include "stdlib.h"
#include "stdint.h"

#include "utils/assert.h"

typedef enum{
   Used,
   FreeToUse,
   Unallocated
}MemoryStatus;

typedef struct MemoryDescriptor{
   struct MemoryDescriptor *next;
   MemoryStatus status;
}MemoryDescriptor;

typedef struct{
   MemoryArea (*allocate)(void);
   MemoryArea (*free)(MemoryArea);
   MemoryDescriptor *memory;
   MemoryDescriptor rootMemoryDescriptor;
}MemoryRoot;

static bool isValidConstraint(unsigned int size, unsigned int alignment, unsigned int boundary);
static unsigned int getOffsetToSatisfyConstraints(uintptr_t address, unsigned int size, unsigned int alignment, unsigned int boundary);

static void mergeWithSurrounding(MemoryRoot *root, MemoryDescriptor *descriptor);
static void *allocateInDescriptorAt(MemoryDescriptor *descriptor, unsigned int size, unsigned int offset);
static void *allocateInDescriptor(MemoryDescriptor *descriptor, unsigned int size);
static unsigned int getDescriptorSize(MemoryDescriptor *descriptor);

static MemoryDescriptor *getPreviousDescriptor(MemoryRoot *root, MemoryDescriptor *descriptor);
static bool isAllocatedMemory(MemoryRoot *root, void *ptr);

static inline void *getMemoryAddress(MemoryDescriptor *descriptor);
static MemoryDescriptor *getDescriptor(void *ptr);

static bool isPowerOf2(unsigned int value);

Memory *memory_new(void *start, unsigned int length){
   if(length <= sizeof(MemoryRoot)){
      return 0;
   }
   MemoryRoot *root = start;
   memset(root, 0, sizeof(MemoryRoot));

   MemoryDescriptor *lastDescriptor = (MemoryDescriptor *)((uint8_t *)start + length) - 1;
   *lastDescriptor = (MemoryDescriptor){
      .status = Unallocated,
      .next = 0
   };
   root->rootMemoryDescriptor = (MemoryDescriptor){
      .next = lastDescriptor,
      .status = FreeToUse,
   };

   root->memory = &root->rootMemoryDescriptor;

   return (Memory *)(root);
}

static void mergeWithNextLeft(MemoryDescriptor *descriptor){
   MemoryDescriptor *next = descriptor->next;
   if(descriptor + 1 == next){
      *descriptor = *next;
   }
}

static MemoryDescriptor *mergeWithNextRight(MemoryDescriptor *descriptor){
   MemoryDescriptor *next = descriptor->next;
   if(descriptor + 1 == next){
      return next;
   }

   return descriptor;
}

static void hole(Memory *memory, MemoryDescriptor *descriptor, uintptr_t address, unsigned int size){
   MemoryDescriptor *lastDescriptor = (MemoryDescriptor *)(address + size - sizeof(MemoryDescriptor));
   MemoryDescriptor *newDescriptor = (MemoryDescriptor *)address;

    *lastDescriptor = (MemoryDescriptor){
       .next = descriptor->next,
       .status = Unallocated,
    };
    lastDescriptor = mergeWithNextRight(lastDescriptor);

//     *newDescriptor = (MemoryDescriptor){
//        .next = lastDescriptor,
//        .status = Used,
//     };
//     descriptor->next = newDescriptor;
//     mergeWithNextLeft(descriptor);


            if(newDescriptor - 1 == descriptor){
               newDescriptor = descriptor;
               *newDescriptor = (MemoryDescriptor){
                  .next = lastDescriptor,
                  .status = Used
               };
            }else{
               *descriptor = (MemoryDescriptor){
                  .next = newDescriptor,
                  .status = Unallocated,
               };
               *newDescriptor = (MemoryDescriptor){
                  .next = lastDescriptor,
                  .status = Used
               };
            }
            memory_free(memory, getMemoryAddress(newDescriptor));
}

bool memory_append(Memory *memory, void *start, unsigned int size){
   if(size <= sizeof(MemoryDescriptor) * 2){
      loggError("Unable to append memory as small as %d bytes", size);
      return false;
   }

   uintptr_t address = (uintptr_t)start;
   MemoryDescriptor *lastDescriptor = (MemoryDescriptor *)(address + size - sizeof(MemoryDescriptor));
   MemoryDescriptor *newDescriptor = start;

   //   f      u     f
   //10 -> 20 -> 30 -> 40
   MemoryRoot *root = (MemoryRoot *)memory;

   uintptr_t rootAddr = (uintptr_t)root;
   if(address < rootAddr && address + size > rootAddr){
      loggError("Overlaping memory, unable to append");
      return false;
   }

   uintptr_t firstArea = (uintptr_t)root->memory;
   if(address < firstArea){
      uintptr_t memEnd = firstArea;
      if(address + size > memEnd){
         loggError("Overlaping memory, unable to append");
         return false;
      }

      if(lastDescriptor + 1 == root->memory){
         lastDescriptor = root->memory;
      }else{
         *lastDescriptor = (MemoryDescriptor){
            .next = root->memory,
            .status = Unallocated
         };
      }

      *newDescriptor = (MemoryDescriptor){
         .next = lastDescriptor,
         .status = Used,
      };

      root->memory = newDescriptor;

      memory_free(memory, getMemoryAddress(newDescriptor));

      return true;
   }

   for(MemoryDescriptor *desc = root->memory; desc != 0; desc = desc->next){
      uintptr_t memStart = (uintptr_t)desc;
      if(desc->next){
         uintptr_t memEnd = (uintptr_t)desc->next + sizeof(MemoryDescriptor);
         if(address < memEnd && address + size >= memEnd){
            loggError("Overlaping memory, unable to append");
            return false;
         }
         if(address >= memStart && address + size <= memEnd){
            if(desc->status != Unallocated){
               loggError("Memory already in use, unable to append");
               return false;
            }

//             if(lastDescriptor + 1 == desc->next){
//                lastDescriptor = desc->next;
//             }else{
//                *lastDescriptor = (MemoryDescriptor){
//                   .next = desc->next,
//                   .status = Unallocated
//                };
//             }

//             if(newDescriptor - 1 == desc){
//                newDescriptor = desc;
//                *newDescriptor = (MemoryDescriptor){
//                   .next = lastDescriptor,
//                   .status = Used
//                };
//             }else{
//                *desc = (MemoryDescriptor){
//                   .next = newDescriptor,
//                   .status = Unallocated,
//                };
//                *newDescriptor = (MemoryDescriptor){
//                   .next = lastDescriptor,
//                   .status = Used
//                };
//             }
            hole(memory, desc, address, size);

            return true;
         }
      }
      else{
         *lastDescriptor = (MemoryDescriptor){
            .next = 0,
            .status = Unallocated
         };
         if(newDescriptor - 1 == desc){
            newDescriptor = desc;
            *newDescriptor = (MemoryDescriptor){
               .next = lastDescriptor,
               .status = Used
            };
         }else{
            desc->next = newDescriptor;
            *newDescriptor = (MemoryDescriptor){
               .next = lastDescriptor,
               .status = Used
            };
         }

         memory_free(memory, getMemoryAddress(newDescriptor));

         return true;
      }
   }

   return false;
}

void *memory_malloc(Memory *memory, unsigned int size){
   MemoryRoot *root = (MemoryRoot *)memory;

   for(MemoryDescriptor *desc = root->memory; desc != 0; desc = desc->next){
      if(desc->status == FreeToUse && getDescriptorSize(desc) >= size){
         return allocateInDescriptor(desc, size);
      }
   }

   return 0;
}

void *memory_calloc(Memory *memory, unsigned size){
   void *result = memory_malloc(memory, size);
   if(result){
      memset(result, 0, size);
   }
   return result;
}

bool memory_canFree(Memory *memory);

static inline uintptr_t getNextAlligned(uintptr_t address, unsigned int alignment){
   return address % alignment == 0 ? 
      address : 
      address + (alignment - address % alignment);
}

static inline uintptr_t getNextClearedBoundary(uintptr_t address, unsigned int size, unsigned int boundary){
   if(boundary == 0){
      return address;
   }

   uintptr_t modStart = address % boundary;  
   uintptr_t modEnd = modStart + size;

   return modEnd > boundary ? getNextAlligned(address, boundary) : address;
}

void *memory_mallocco(Memory *memory, unsigned size, unsigned alignment, unsigned boundary){
   if(!isValidConstraint(size, alignment, boundary)){
      return 0;
   }

   MemoryRoot *root = (MemoryRoot *)memory;

   for(MemoryDescriptor *desc = root->memory; desc != 0; desc = desc->next){
      if(desc->status != FreeToUse){
         continue;
      }

      uintptr_t address = (uintptr_t)getMemoryAddress(desc);
      uintptr_t offset = getOffsetToSatisfyConstraints(address, size, alignment, boundary);

      if(getDescriptorSize(desc) >= size + offset){
         return allocateInDescriptorAt(desc, size, offset);
      }
   }

   return 0;
}

static bool isValidConstraint(unsigned int size, unsigned int alignment, unsigned int boundary){
   if(!isPowerOf2(alignment)){
      loggError("Alignment must be a power or 2");
      return false;
   }
   if(boundary != 0 && !isPowerOf2(boundary)){
      loggError("Boundary must be a power or 2");
      return false;
   }
   if(boundary != 0 && size > boundary){
      loggError("Size can not be greater than boundary restriction");
      return false;
   }

   return true;
}

static unsigned int getOffsetToSatisfyConstraints(uintptr_t address, unsigned int size, unsigned int alignment, unsigned int boundary){
      uintptr_t nextAlligned = getNextAlligned(address, alignment);
      uintptr_t nextCleared = getNextClearedBoundary(nextAlligned, size, boundary);
      return nextCleared - address;
}

void *memory_callocco(
      Memory *memory,
      unsigned int size,
      unsigned int alignment,
      unsigned int boundary){

   void *result = memory_mallocco(memory, size, alignment, boundary);
   if(result){
      memset(result, 0, size);
   }
   return result;
}

void memory_free(Memory *memory, void *ptr){
   MemoryRoot *root = (MemoryRoot *)memory;

   if(!isAllocatedMemory(root, ptr)){
      loggError("Unable to free %X, memory not allocated", ptr);
      return;
   }

   MemoryDescriptor *descriptor = getDescriptor(ptr);
   descriptor->status = FreeToUse;
   mergeWithSurrounding(root, descriptor);
}

void memory_addAllocator(MemoryArea (*allocate)(void), void (*free)(MemoryArea));

static void mergeWithSurrounding(MemoryRoot *root, MemoryDescriptor *descriptor){
   MemoryDescriptor *previous = getPreviousDescriptor(root, descriptor);

   if(descriptor->next && descriptor->next->status == FreeToUse){
      descriptor->next = descriptor->next->next;
   }

   if(previous && previous->status == FreeToUse){
      previous->next = descriptor->next;
   }
}

static void *allocateInDescriptorAt(MemoryDescriptor *descriptor, unsigned int size, unsigned int offset){
   if(offset > sizeof(MemoryDescriptor)){
      allocateInDescriptor(descriptor, offset - sizeof(MemoryDescriptor));
      descriptor->status = FreeToUse;

      return allocateInDescriptor(descriptor->next, size);
   }
   else{
      uintptr_t addr = (uintptr_t)allocateInDescriptor(descriptor, size + offset);
      return (void*)(addr + offset);
   }
}

static void *allocateInDescriptor(MemoryDescriptor *descriptor, unsigned int size){
   unsigned int descriptorSize = getDescriptorSize(descriptor);
   assert(descriptorSize >= size);
   assert(descriptor->used == false);

   if(descriptorSize > size + sizeof(MemoryDescriptor)){
      uintptr_t address = (uintptr_t)descriptor;
      MemoryDescriptor *newDescriptor = (MemoryDescriptor *)(address + sizeof(MemoryDescriptor) + size);
      *newDescriptor = (MemoryDescriptor){
         .next = descriptor->next,
         .status = FreeToUse
      };
      *descriptor = (MemoryDescriptor){
         .next = newDescriptor,
         .status = Used,
      };
   }
   else{
      descriptor->status = Used;      
   }

   return getMemoryAddress(descriptor);
}

static MemoryDescriptor *getPreviousDescriptor(MemoryRoot *root, MemoryDescriptor *descriptor){
   MemoryDescriptor *prev = 0;
   for(MemoryDescriptor *curr = root->memory; curr != 0; curr = curr->next){
      if(curr == descriptor){
         return prev;
      }
      prev = curr;
   }

   return 0;
}

static bool isAllocatedMemory(MemoryRoot *root, void *ptr){
   MemoryDescriptor *expected = getDescriptor(ptr);
   for(MemoryDescriptor *curr = root->memory; curr != 0; curr = curr->next){
      if(curr == expected){
         return true;
      }
   }

   return false;
}

static inline MemoryDescriptor *getDescriptor(void *ptr){
   return (MemoryDescriptor *)((uintptr_t)ptr - sizeof(MemoryDescriptor));
}

static inline void *getMemoryAddress(MemoryDescriptor *descriptor){
   return (void*)((uintptr_t)descriptor + sizeof(MemoryDescriptor));
}

static unsigned int getDescriptorSize(MemoryDescriptor *descriptor){
   if(descriptor->next == 0){
      return 0;
   } 

   uintptr_t address1 = (uintptr_t)descriptor;
   uintptr_t address2 = (uintptr_t)descriptor->next;

   return address2 - address1 - sizeof(MemoryDescriptor);
}

static bool isPowerOf2(unsigned int value){
   if(value == 0){
      return false;
   }

   while((value & 1) == 0){
      value >>= 1;
   }
   value >>= 1;

   return value == 0;
}
