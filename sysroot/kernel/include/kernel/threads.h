#ifndef THREADS_H_INCLUDED
#define THREADS_H_INCLUDED

#include "stdint.h"

typedef struct{
   void (*start)(void *data);
   void *data;
   uint32_t cs;
   uint32_t ss;
   uint32_t esp;
   uint32_t eflags;
}ThreadConfig;

void threads_init();
void thread_start(ThreadConfig config);

#endif
