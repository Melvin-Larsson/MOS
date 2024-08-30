#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include "stdint.h"
#include "stdbool.h"

typedef enum{
   TimerOk,
   TimerAlreadyStarted,
}TimerStatus;

typedef enum{
   Instant,
   Eventual 
}TimerPriority;

typedef struct{
   void *data;
}Timer;

typedef struct{
   void (*handler)(void *data);
   void *data;
   uint64_t timeNanos;
   bool repeat;
   TimerPriority priority;
}TimerConfig;

typedef struct{
   void (*handler)(void);
   uint64_t timeNanos;
   bool repeat;
}CriticalTimerConfig;

typedef struct{
   void *data;
}CriticalTimer;

void timers_init();
TimerConfig timer_createDefaultConfig(void (*handler)(void *data), void *data, uint64_t timeNanos);
Timer *timer_new(TimerConfig config);
TimerStatus timer_start(Timer *timer);
void timer_stop(Timer *timer);
void timer_free(Timer *timer);
bool timers_freeAll();

CriticalTimerConfig criticalTimer_createDefaultConfig(void (*handler)(), uint64_t timeNanos);
CriticalTimer *criticalTimer_new(CriticalTimerConfig config);
bool criticalTimer_start(CriticalTimer *criticalTimer);
bool criticalTimer_stop(CriticalTimer *criticalTimer);
void criticalTimer_checkoutInterrupt(CriticalTimer *criticalTimer);
void criticalTimer_free(CriticalTimer *criticalTimer);

#endif
