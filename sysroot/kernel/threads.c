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

typedef enum{
   Running,
   Waiting
}ThreadStatus;

typedef struct{
   StackFrame stackFrame;
   ThreadStatus status;
}Thread;

typedef struct ThreadList{
   struct ThreadList *next;
   Thread *thread;
}ThreadListNode;

typedef volatile struct{
   unsigned int count;
   ThreadListNode *waitingThreads;
}SemaphoreData;

static ThreadListNode* append(ThreadListNode *root, Thread *thread);
static ThreadListNode *removeFirst(ThreadListNode *root);
static ThreadListNode *remove(ThreadListNode *root, ThreadListNode *node);

static void aquireLock();
static void releaseLock();

static void scheduleThread(Thread *thread);

static ThreadListNode *runningThreads;
static ThreadListNode *activeThread;

void threads_init(){
   runningThreads = 0;
   activeThread = 0;

   Thread *thread = calloc(sizeof(Thread));
   thread->status = Running;

   scheduleThread(thread);
   pit_setTimer(0,0,0);
}

void thread_start(ThreadConfig config){
   aquireLock();
   Thread *thread = calloc(sizeof(Thread));

   thread->stackFrame = (StackFrame){
      .eip = (uint32_t)config.start,
         .eax = (uint32_t)config.data,
         .cs = config.cs,
         .ss = config.ss,
         .esp = config.esp,
         .eflags = config.eflags
   };
   thread->status = Running;

   scheduleThread(thread);
   releaseLock();
}

Semaphore *semaphore_new(unsigned int count){
   aquireLock();
   SemaphoreData *semaphoreData = malloc(sizeof(SemaphoreData));
   *semaphoreData = (SemaphoreData){
      .count = count,
      .waitingThreads = 0
   };

   Semaphore *semaphore = malloc(sizeof(Semaphore));
   *semaphore = (Semaphore){
      .data = (void*)semaphoreData
   };
   releaseLock();
   return semaphore;
}

void semaphore_aquire(Semaphore *semaphore){
   SemaphoreData *data = semaphore->data;

   aquireLock();

   if(data->count == 0){
      data->waitingThreads = append(data->waitingThreads, activeThread->thread);
      activeThread->thread->status = Waiting;
      releaseLock();
      while(data->count == 0);
   }

   data->count--;
   releaseLock();
}

void semaphore_release(Semaphore *semaphore){
   aquireLock();

   SemaphoreData *data = semaphore->data;
   data->count++;

   if(data->waitingThreads){
      data->waitingThreads->thread->status = Running;
      scheduleThread(data->waitingThreads->thread);
      data->waitingThreads = removeFirst(data->waitingThreads);
   }

   releaseLock();
}

static void scheduleThread(Thread *thread){
   ThreadListNode *node = malloc(sizeof(ThreadListNode));
   node->thread = thread;

   if(!activeThread){
      activeThread = node;
   }

   if(!runningThreads){
      runningThreads = node;
      node->next = 0;
      return;
   }

   node->next = runningThreads;
   runningThreads = node;
}

static inline void aquireLock(){
   __asm__ volatile("cli");
}
static inline void releaseLock(){
   __asm__ volatile("sti");
}

static ThreadListNode *append(ThreadListNode *root, Thread *thread){
   ThreadListNode *node = malloc(sizeof(ThreadListNode));
   *node = (ThreadListNode){
      .next = 0,
      .thread = thread
   };

   if(!root){
      return node;
   }

   ThreadListNode *curr = root;
   while(curr->next){
      curr = curr->next;
   }
   curr->next = node;

   return root;
}

static ThreadListNode *removeFirst(ThreadListNode *root){
   ThreadListNode *next = root->next;
   free(root);
   return next;
}

static ThreadListNode *remove(ThreadListNode *root, ThreadListNode *node){
   if(node == root){
      return removeFirst(root);
   }

   ThreadListNode *curr = root;
   while(curr != node && curr != root){
      curr = curr->next;
   }

   if(curr == root){
      return root;
   }

   curr->next = curr->next->next;
   free(node);
   return root;
}

#define APIC_EOI_ADDRESS 0xFEE000B0
void task_switch_handler(StackFrame *stackFrame){
   if(activeThread){ 
      activeThread->thread->stackFrame = *stackFrame;

      ThreadListNode *nextThread = activeThread->next ? activeThread->next : runningThreads;
      if(activeThread->thread->status == Waiting){
         runningThreads = remove(runningThreads, activeThread);
      }

      activeThread = nextThread;
      if(activeThread){
         *stackFrame = activeThread->thread->stackFrame;
      }
   }else if(runningThreads){
      activeThread = runningThreads;
      *stackFrame = activeThread->thread->stackFrame;
   }

   uint32_t eoiData = 0;
   paging_writePhysicalOfSize(APIC_EOI_ADDRESS, &eoiData, 4, AccessSize32);

   pit_setTimer(0,0,0);
}

