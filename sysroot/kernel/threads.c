#include "kernel/threads.h"
#include "kernel/timer.h"
#include "kernel/paging.h"
#include "kernel/memory.h"
#include "stdbool.h"

#define THREAD_SWITCH_DELAY_MILLIS 10

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
   Waiting,
   Sleeping
}ThreadStatus;

typedef volatile struct{
   uint32_t esp;
   ThreadStatus status;
   unsigned int sleepTimeMillis;
}Thread;

typedef volatile struct ThreadListNode{
   volatile struct ThreadListNode *next;
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
static void updateSleepingThreads(unsigned int timePassedMillis);
static ThreadListNode* addThread(Thread *thread, ThreadListNode *list);

extern void task_switch_handler(void);

static ThreadListNode *runningThreads;
static ThreadListNode *activeThread;
static ThreadListNode *sleepingThreads;
static CriticalTimer *timer;

ThreadsStatus threads_init(){
   CriticalTimerConfig cconfig = criticalTimer_createDefaultConfig(task_switch_handler, THREAD_SWITCH_DELAY_MILLIS * 1000 * 1000);
   cconfig.repeat = true;
   timer = criticalTimer_new(cconfig);
   if(!timer){
      return ThreadsUnableToAquireTimer;
   }

   runningThreads = 0;
   activeThread = 0;
   sleepingThreads = 0;

   Thread *thread = kcalloc(sizeof(Thread));
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
   Thread *thread = kcalloc(sizeof(Thread));

   StackFrame stackFrame = {
      .eip = (uint32_t)config.start,
      .eax = (uint32_t)config.data,
      .cs = config.cs,
      .eflags = config.eflags
   };
   uint32_t *ptr = (uint32_t *)&stackFrame.eflags;
   uint32_t *stack = (uint32_t *)config.esp;
   for(unsigned int i = 0; i < sizeof(StackFrame)/sizeof(uint32_t); i++){
      stack =  push(stack, *ptr--);
   }
   thread->esp = (uint32_t)stack;

   thread->status = Running;

   scheduleThread(thread);
   releaseLock();
}

void thread_sleep(unsigned int millis){
   if(millis == 0){
      return;
   }

   Thread *thread = activeThread->thread;
   thread->status = Sleeping;
   thread->sleepTimeMillis = millis;
   sleepingThreads = addThread(thread, sleepingThreads);
   while(thread->status == Sleeping); //FIXME: Should be able to switch thread imidiatelly
                                      //Also, the time passed here is not counted towards the sleep time
}

Semaphore *semaphore_new(unsigned int count){
   aquireLock();
   SemaphoreData *semaphoreData = kmalloc(sizeof(SemaphoreData));
   *semaphoreData = (SemaphoreData){
      .count = count,
         .waitingThreads = 0
   };

   Semaphore *semaphore = kmalloc(sizeof(Semaphore));
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
   ThreadListNode *node = kmalloc(sizeof(ThreadListNode));
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
   ThreadListNode *node = kmalloc(sizeof(ThreadListNode));
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
   kfree((void*)root);
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
   kfree((void*)node);
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

   updateSleepingThreads(THREAD_SWITCH_DELAY_MILLIS);
   criticalTimer_checkoutInterrupt(timer);

   return newEsp;
}

static void updateSleepingThreads(unsigned int timePassedMillis){
   ThreadListNode dummy = {
      .next = sleepingThreads
   };

   ThreadListNode *prev = &dummy;
   ThreadListNode *node = sleepingThreads;
   while(node){
      Thread *thread = node->thread;
      if(thread->sleepTimeMillis < timePassedMillis){
         thread->sleepTimeMillis = 0;
         thread->status = Running;
         scheduleThread(thread);
         prev->next = node->next;       
         kfree((void*)node);
         node = prev->next;
      }
      else{
         thread->sleepTimeMillis -= timePassedMillis;
         node = node->next;
      }
   }

   sleepingThreads = dummy.next;
}

static ThreadListNode* addThread(Thread *thread, ThreadListNode *list){
   ThreadListNode *node = kmalloc(sizeof(ThreadListNode));
   *node = (ThreadListNode){
      .next = list,
      .thread = thread
   };
   return node;
}
