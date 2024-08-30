#ifndef THREADS_H_INCLUDED
#define THREADS_H_INCLUDED

#include "stdint.h"

typedef enum{
   ThreadsOk,
   ThreadsUnableToAquireTimer
}ThreadsStatus;

typedef struct{
   void (*start)(void *data);
   void *data;
   uint32_t cs;
   uint32_t ss;
   uint32_t esp;
   uint32_t eflags;
}ThreadConfig;

typedef struct{
   void *data;
}Semaphore;

ThreadsStatus threads_init();
void thread_start(ThreadConfig config);

Semaphore *semaphore_new(unsigned int count);
void semaphore_aquire(Semaphore *semaphore);
void semaphore_release(Semaphore *semaphore);

#endif
