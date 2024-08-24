#include "kernel/threads.h"
#include "kernel/pit.h"
#include "kernel/paging.h"
#include "stdlib.h"
#include "stdbool.h"

typedef struct{
   uint32_t edi;
   uint32_t esi;
   uint32_t ebp;
   uint32_t reserved;
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax;

   uint32_t eip;
   uint32_t cs;
   uint32_t eflags;
   uint32_t esp;
   uint32_t ss;
}StackFrame;

typedef struct Thread{
   struct Thread *next;
   StackFrame stackFrame;
}Thread;

static Thread *currentThread;

void threads_init(){
   currentThread = calloc(sizeof(Thread));
   currentThread->next = currentThread;
}

void thread_start(ThreadConfig config){
   Thread *thread = calloc(sizeof(Thread));

   thread->stackFrame = (StackFrame){
      .eip = (uint32_t)config.start,
         .eax = (uint32_t)config.data,
         .cs = config.cs,
         .ss = config.ss,
         .esp = config.esp,
         .eflags = config.eflags
   };

   bool is2ndThread = currentThread == currentThread->next;
   thread->next = currentThread->next;
   currentThread->next = thread;

   if(is2ndThread){
      pit_setTimer(0,0,0);
   }
}

#define APIC_EOI_ADDRESS 0xFEE000B0
void task_switch_handler(StackFrame *stackFrame){
   currentThread->stackFrame = *stackFrame;
   currentThread = currentThread->next;
   *stackFrame = currentThread->stackFrame;

   uint32_t eoiData = 0;
   paging_writePhysicalOfSize(APIC_EOI_ADDRESS, &eoiData, 4, AccessSize32);

   pit_setTimer(0,0,0);
}

