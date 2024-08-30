#include "kernel/threads.h"
#include "kernel/timer.h"
#include "kernel/paging.h"
#include "stdlib.h"
#include "stdbool.h"

typedef volatile struct{
   uint32_t edi;
   uint32_t esi;
   uint32_t ebp;
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax;

   uint32_t eip;
   uint32_t cs;
   uint32_t eflags;
}StackFrame;

typedef enum{
   Running,
   Waiting
}ThreadStatus;

typedef volatile struct{
   uint32_t esp;
   ThreadStatus status;
}Thread;

typedef volatile struct ThreadList{
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

extern void task_switch_handler(void);

static ThreadListNode *runningThreads;
static ThreadListNode *activeThread;
static CriticalTimer *timer;

ThreadsStatus threads_init(){
   CriticalTimerConfig cconfig = criticalTimer_createDefaultConfig(task_switch_handler, 10000000);
   cconfig.repeat = true;
   timer = criticalTimer_new(cconfig);
   if(!timer){
      return ThreadsUnableToAquireTimer;
   }

   runningThreads = 0;
   activeThread = 0;

   Thread *thread = calloc(sizeof(Thread));
   thread->status = Running;

   scheduleThread(thread);
   criticalTimer_start(timer);
   return ThreadsOk;
}

static uint32_t *push(uint32_t *stack, uint32_t value){
   *--stack = value;
   return stack;
}

void thread_start(ThreadConfig config){
   aquireLock();
   Thread *thread = calloc(sizeof(Thread));

   StackFrame stackFrame = {
      .eip = (uint32_t)config.start,
      .eax = (uint32_t)config.data,
      .cs = config.cs,
      .eflags = config.eflags
   };
   uint32_t *ptr = (uint32_t *)&stackFrame.eflags;
   uint32_t *stack = (uint32_t *)config.esp;
   for(int i = 0; i < sizeof(StackFrame)/sizeof(uint32_t); i++){
      stack =  push(stack, *ptr--);
   }
   thread->esp = (uint32_t)stack;

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
      while(1){
         if(data->count > 0){
            aquireLock();
            if(data->count > 0){
               break;
            }
            releaseLock();
         }
      }
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

uint32_t thread_getNewEsp(uint32_t esp){
   uint32_t newEsp = esp;

   if(activeThread){ 
      activeThread->thread->esp = esp;

      ThreadListNode *nextThread = activeThread->next ? activeThread->next : runningThreads;
      if(activeThread->thread->status == Waiting){
         runningThreads = remove(runningThreads, activeThread);
      }

      if(nextThread){
         activeThread = nextThread;
         newEsp = activeThread->thread->esp;
      }
   }
   else if(runningThreads){
      activeThread = runningThreads;
   }

   criticalTimer_checkoutInterrupt(timer);

   return newEsp;
}
