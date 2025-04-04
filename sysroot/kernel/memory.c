#include "kernel/memory.h"
#include "kernel/logging.h"
#include "stdint.h"
#include "stdlib.h"

typedef struct MEMORY{
    uint8_t used;
    struct MEMORY *next;
}MemoryDescriptor;

static int useDescriptor(MemoryDescriptor *descriptor, int size);
static int isValidConstraint(int size, int alignment, int boundary);
static MemoryDescriptor *constrainDescriptor(MemoryDescriptor *descriptor, int size, int alignment, int boundary);

static int hasEnoughSpace(MemoryDescriptor *descriptor, MemoryDescriptor *next,  unsigned int size);
static int hasExtraSpaceAfter(MemoryDescriptor *descriptor, unsigned int size);
static int hasExtraSpaceBefore(MemoryDescriptor *descriptor, unsigned int alignment);

static uint8_t *avoidBoundary(void *ptr, int size, int boundary);
static uint8_t *getNextAlligned(void *ptr, int alignment);

static int isCrossingBoundary(void *ptr, unsigned int size, int boundary);

static void *getMemoryPointer(MemoryDescriptor *descriptor);

static MemoryDescriptor *memoryDescriptor;

void memory_init(){
    memoryDescriptor = (MemoryDescriptor*)0x200000; //FIXME
    *memoryDescriptor = (MemoryDescriptor){0,0};
}

void debug_logMemory(){
    loggDebug("__Dynamic Memory__");
    for(MemoryDescriptor *desc = memoryDescriptor; desc != 0; desc = desc->next){
        loggDebug("(%X:%d) ", getMemoryPointer(desc), desc->used);
    }
}

void *kcalloc(int size){
    void* ptr = kmalloc(size);
    memset(ptr, 0, size);
    return ptr;
}
void *kcallocco(int size, int alignment, int boundaries){
    void *ptr = kmallocco(size, alignment, boundaries);
    memset(ptr, 0, size);
    return ptr;
}

void *kmalloc(int size){
    for(MemoryDescriptor *desc = memoryDescriptor; desc != 0; desc = desc->next){
        if(useDescriptor(desc, size)){
            return getMemoryPointer(desc);
        }
    }
    return 0;
}
void *kmallocco(int size, int alignment, int boundary){
    if(!isValidConstraint(size, alignment, boundary)){
        loggError("Invalid constraint %d %d %d", size, alignment, boundary);
        return 0;
    }
    MemoryDescriptor *last = 0;
    for(MemoryDescriptor *desc = memoryDescriptor; desc != 0; desc = desc->next){
        if(!desc->used){
            MemoryDescriptor *constrained = constrainDescriptor(desc, size, alignment, boundary);
            if(constrained != 0){
                if(hasExtraSpaceBefore(desc, alignment)){
                    desc->next = constrained;
                }
                else if(last != 0){
                    last->next = constrained;
                }else{
                    memoryDescriptor = constrained;
                }
                desc = constrained;
                useDescriptor(desc, size);
                return getMemoryPointer(desc);
            }
        }
        last = desc;
    }

    loggWarning("Out of memory");
    return 0;
}
void kfree(void *ptr){
    MemoryDescriptor *last = 0;
    for(MemoryDescriptor *desc = memoryDescriptor; desc != 0; desc = desc->next){
        if((uint8_t*)desc + sizeof(MemoryDescriptor) == ptr){
            MemoryDescriptor *start = desc;
            MemoryDescriptor *end = desc->next;
            if(last && !last->used){
                start = last;
            }
            if(desc->next && !desc->next->used){
                end = desc->next->next;
            }
            start->used = 0;
            start->next = end;
            return;
        }
        last = desc;
    }
}


static int useDescriptor(MemoryDescriptor *descriptor, int size){
    if(descriptor->used){
        return 0;
    }
    if(hasExtraSpaceAfter(descriptor, size)){
        uint8_t *curr = (uint8_t*)descriptor;
        MemoryDescriptor *newNext = (MemoryDescriptor*)(curr + sizeof(MemoryDescriptor) + size);
        newNext->next = descriptor->next;
        newNext->used = 0;
        descriptor->next = newNext;
        descriptor->used = 1;
        return 1;
    }
    if(hasEnoughSpace(descriptor, descriptor->next, size)){
        descriptor->used = 1;
        return 1;
    }
    return 0;
} 


static int isValidConstraint(int size, int alignment, int boundary){
    if(boundary == 0){
        return 1;
    }
    int boundaryToAlignmentOffset = (alignment - boundary % alignment) % alignment;
    return size + boundaryToAlignmentOffset <= boundary;
}
static MemoryDescriptor *constrainDescriptor(MemoryDescriptor *descriptor, int size, int alignment, int boundary){
    MemoryDescriptor *bounded = (MemoryDescriptor*)avoidBoundary(descriptor, size, boundary);
    MemoryDescriptor *aligned = (MemoryDescriptor*)getNextAlligned(bounded, alignment);
    if(hasEnoughSpace(aligned, descriptor->next, size)){
        *aligned = *descriptor;
        return aligned;
    }
    return 0;
}

static int hasEnoughSpace(MemoryDescriptor *descriptor, MemoryDescriptor *next,  unsigned int size){
    if(next == 0){
        return 1;
    }
    int space = (int)((uint8_t*)next - (uint8_t*)descriptor);
    return space >= (int)(size + sizeof(MemoryDescriptor));
}
static int hasExtraSpaceAfter(MemoryDescriptor *descriptor, unsigned int size){
    if(descriptor->next == 0){
        return 1;
    }
    int space = (int)((uint8_t*)descriptor->next - (uint8_t*)descriptor);
    return space >= (int)(size + 2 * sizeof(MemoryDescriptor));
}
static int hasExtraSpaceBefore(MemoryDescriptor *descriptor, unsigned int alignment){
    int space = (int)(getNextAlligned(descriptor, alignment) - (uint8_t*)descriptor);
    return space > (int)sizeof(MemoryDescriptor);
}


static uint8_t *avoidBoundary(void *ptr, int size, int boundary){
    if(!isCrossingBoundary(ptr, size, boundary)){
        return ptr;
    }
    return getNextAlligned(ptr, boundary);
}


static uint8_t *getNextAlligned(void *ptr, int alignment){
    int offset = alignment - (((uintptr_t)ptr + sizeof(MemoryDescriptor)) % alignment);
    offset %= alignment;
    return (uint8_t*)ptr + offset;
}
static int isCrossingBoundary(void *ptr, unsigned int size, int boundary){
    if(boundary == 0){
        return 0;
    }
    uintptr_t addr = (uintptr_t)ptr + sizeof(MemoryDescriptor);
    return addr / boundary != ((uint64_t)addr + size - 1) / boundary;
}
static void *getMemoryPointer(MemoryDescriptor *descriptor){
    return (uint8_t*)descriptor + sizeof(MemoryDescriptor);
}
