#include "kernel/task-structures.h"
#include "kernel/task.h"
#include "stdlib.h"
#include "kernel/logging.h"
#include "kernel/descriptors.h"

typedef volatile struct{
   uint64_t segmentLimitLow : 16;
   uint64_t baseAddressLow : 24;
   uint64_t segmentType : 4;
   uint64_t descriptorType : 1;
   uint64_t descriptorPrivilegeLevel : 2;
   uint64_t segmentPresent : 1;
   uint64_t segmentLimitHigh : 4;
   uint64_t avl : 1;
   uint64_t is64BitCodeSegment : 1;
   uint64_t is32BitSegment : 1;
   uint64_t granularity : 1;
   uint64_t baseAddressHigh : 8;
}__attribute__((packed))SegmentDescriptor;

extern volatile SegmentDescriptor tss;

static void func(){
   loggInfo("Hello from another task!\n");
   while(1);
}

#define VIDEO_MEMORY ((uint16_t*)0xb8000)
static void test(){
    VIDEO_MEMORY[1] = 15 << 8 | 'x';
    while(1);
}

void initKernelTask(uintptr_t stack){
   TaskStateSegment32 *tssSegment = callocco(sizeof(TaskStateSegment32), 4096, 0);
   tssSegment->ss0 = (2 << 3 | 0);
   tssSegment->esp0 = stack;

   loggDebug("Add tss to gdt");
   tss.baseAddressLow = (uintptr_t)tssSegment & 0xFFFFFF;
   tss.baseAddressHigh = (uintptr_t)tssSegment >> 24;
   loggInfo("Tss at %X added to gdt", tssSegment);

   __asm__ volatile("ltr %%ax"
         :
         : "ax"(5 << 3));
}

static void initCurrTask(){
   TaskStateSegment32 *tss = malloc(sizeof(TaskStateSegment32) + 32);

   *tss = (TaskStateSegment32){
      .ioMapBaseAddress = sizeof(TaskStateSegment32) + 32,
      .esp0 = 0x18FECA,
      .ss0 = 0x10,

      .esp1 = 0x18FECA,
      .ss1 = 0x10,

      .esp2 = 0x18FECA,
      .ss2 = 0x10,

//       .ssp = 1,
      
      .T = 0
   };
   loggInfo("tss %X\n", tss);
   GdtTssDescriptor tssd  = {
      .address = (uintptr_t)tss,
      .size = sizeof(TaskStateSegment32) + 32,
      .use4KBGranularity = 0,
      .descriptorPrivilegeLevel = 0
   };
   uint16_t segmentSelector = gdt_addTss32Descriptor(tssd) << 3;
   loggInfo("sect %X\n", segmentSelector);

   __asm__ volatile ("ltr %[selector]"
         :
         : [selector]"r"(segmentSelector)
         :
         );
   loggInfo("done\n");

   loggInfo("func at %X\n", func);
   loggInfo("task_test %X\n", task_test);

   
//    uint32_t *data = malloc(4 * 3);

//    volatile uint32_t *p1 = data;
//    volatile uint32_t *p2 = &data[1];
//    volatile uint32_t *p3 = &data[2];

//    *p1 = 10;
//    *p2 = 0;
//    *p3 = *p1 / *p2;

}

struct far_ptr {
    uint32_t offset;
    uint16_t selector;
} __attribute__((packed));

void task_test(){
   initCurrTask();
//    loggInfo("inited\n");
   TaskStateSegment32 *tss = callocco(sizeof(TaskStateSegment32) + 32, 1024, 1024);

   *tss = (TaskStateSegment32){
      .es = 0x10,
      .cs = 0x8,
      .ss = 0x10,
      .ds = 0x10,
      .fs = 0x10,
      .gs = 0x10,

//       .eax = 0x20,
//       .ebx = 0x18feca,
//       .ecx = 0x50a,
      .edx = 0xb,
//       .esi = 0x18ff54,
//       .edi = 0x8c,
      .esp = 0x18FECA,
//       .ebp = 0x18ff00,
//       .eflags = 0x200202,
      .eflags = 0b100001000000010,


      .eip = (uint32_t)func,
      .ioMapBaseAddress = sizeof(TaskStateSegment32) + 32,
      .esp0 = 0x18FECA,
      .ss0 = 0x10,

      .esp1 = 0x18FECA,
      .ss1 = 0x10,

      .esp2 = 0x18FECA,
      .ss2 = 0x10,

//       .ssp = 1,
      
      .T = 0,
      .cr3 = 1,
      .previousTaskLink = 1

   };

   loggInfo("tss2 %X\n", tss);

   GdtTssDescriptor tssd  = {
      .address = (uintptr_t)tss,
      .size = sizeof(TaskStateSegment32) + 32,
      .use4KBGranularity = 0,
      .descriptorPrivilegeLevel = 0
   };
   volatile uint16_t segmentSelector = gdt_addTss32Descriptor(tssd) << 3;
   loggInfo("segment %X\n", segmentSelector);
   loggInfo("stack %X\n", &segmentSelector);
   uint16_t dest = 0;

   __asm__ volatile ("lar %[src], %[dst]"
         : [dst]"=r"(dest)
         : [src]"r"(segmentSelector)
         : 
         );

   loggInfo("dest %X\n", dest);

   volatile struct far_ptr ptr = {(uint32_t)func, segmentSelector};
//    printvolatile f("seg se: %X\n", segmentSelector);


//    loggInfo("func %X\n", task_test);


//    __asm__(".intel_syntax noprefix");
//    __asm__ volatile("call 0x8:0xf050"
//          :
//          :
//          :
//          );
//    __asm__(".att_syntax prefix");
   __asm__ volatile (
         "lcall *0(%%eax)"
         :
         : "eax"(&ptr)
         : "memory"
         );

//    __asm__ volatile ("call %[selector]"
//             :
//             : [selector]"r"(segmentSelector)
//             :
//             );

   loggInfo("after call");
}
