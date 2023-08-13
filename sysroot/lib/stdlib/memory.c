#include "stdlib.h"
#include "stdint.h"

typedef struct MEMORY{
    uint8_t used;
    struct MEMORY *next;
}MemoryDescriptor;

static int useDescriptor(MemoryDescriptor *descriptor, int size);
static int isValidConstraint(int size, int alignment, int boundary);
static MemoryDescriptor *constrainDescriptor(MemoryDescriptor *descriptor, int size, int alignment, int boundary);

static int hasEnoughSpace(MemoryDescriptor *descriptor, unsigned int size);
static int hasExtraSpaceAfter(MemoryDescriptor *descriptor, unsigned int size);
static int hasExtraSpaceBefore(MemoryDescriptor *descriptor, unsigned int alignment);

static uint8_t *avoidBoundary(void *ptr, int size, int boundary);
static uint8_t *getNextAlligned(void *ptr, int alignment);

static int isCrossingBoundary(void *ptr, unsigned int size, int boundary);

static void *getMemoryPointer(MemoryDescriptor *descriptor);

static MemoryDescriptor *memoryDescriptor;

void stdlib_init(){
    memoryDescriptor = (MemoryDescriptor*)0x70000; //FIXME
    *memoryDescriptor = (MemoryDescriptor){0,0};
}

void *malloc(int size){
    for(MemoryDescriptor *desc = memoryDescriptor; desc != 0; desc = desc->next){
        if(useDescriptor(desc, size)){
            return getMemoryPointer(desc);
        }
    }
    return 0;
}
void *malloc_constrained(int size, int alignment, int boundary){
    if(!isValidConstraint(size, alignment, boundary)){
        return 0;
    }
    MemoryDescriptor *last = 0;
    for(MemoryDescriptor *desc = memoryDescriptor; desc != 0; desc = desc->next){
        if(desc->used){
            continue;
        }
        MemoryDescriptor *constrained = constrainDescriptor(desc, size, alignment, boundary);
        if(constrained != 0){
            constrained->next = desc->next;
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
        last = desc;
    }
    return 0;

}
void free(void *ptr){
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
    if(hasEnoughSpace(descriptor, size)){
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
    if(hasEnoughSpace(aligned, size)){
        return aligned;
    }
    return 0;
}


static int hasEnoughSpace(MemoryDescriptor *descriptor, unsigned int size){
    unsigned int space = (unsigned int)((uint8_t*)descriptor->next - (uint8_t*)descriptor);
    return space >= size + sizeof(MemoryDescriptor);
}
static int hasExtraSpaceAfter(MemoryDescriptor *descriptor, unsigned int size){
    if(descriptor->next == 0){
        return 1;
    }
    unsigned int space = (unsigned int)((uint8_t*)descriptor->next - (uint8_t*)descriptor);
    return space >= size + 2 * sizeof(MemoryDescriptor);
}
static int hasExtraSpaceBefore(MemoryDescriptor *descriptor, unsigned int alignment){
    unsigned int space = (unsigned int)(getNextAlligned(descriptor, alignment) - (uint8_t*)descriptor);
    return space > sizeof(MemoryDescriptor);
}


static uint8_t *avoidBoundary(void *ptr, int size, int boundary){
    if(!isCrossingBoundary(ptr, size, boundary)){
        return ptr;
    }
    return getNextAlligned(ptr, boundary);
}


static uint8_t *getNextAlligned(void *ptr, int alignment){
    int offset = alignment - (((uint64_t)ptr + sizeof(MemoryDescriptor)) % alignment);
    offset %= alignment;
    return (uint8_t*)ptr + offset;
}
static int isCrossingBoundary(void *ptr, unsigned int size, int boundary){
    if(boundary == 0){
        return 0;
    }
    uint64_t addr = (uint64_t)ptr + sizeof(MemoryDescriptor);
    return addr / boundary != ((uint64_t)addr + size - 1) / boundary;
}
static void *getMemoryPointer(MemoryDescriptor *descriptor){
    return (uint8_t*)descriptor + sizeof(MemoryDescriptor);
}
