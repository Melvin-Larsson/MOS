#include "kernel/timer.h"
#include "stdlib.h"

typedef struct TimerData{
   struct TimerData *next;
   TimerConfig config;
   uint64_t timeLeftNanos;
   bool started;
}TimerData;

static TimerData *timers;

void timers_init(){
   timers = 0;
}

TimerConfig timer_createDefaultConfig(void (*handler)(void *data), void *data, uint64_t timeNanos){
   return (TimerConfig){
      .handler = handler,
      .data = data,
      .timeNanos = timeNanos,
      .repeat = false,
      .priority = Eventual
   };
}

Timer *timer_new(TimerConfig config){
   TimerData *timerData = malloc(sizeof(TimerData));
   *timerData = (TimerData){
      .next = 0,
      .config = config,
      .timeLeftNanos = config.timeNanos,
      .started = false,
   };

   Timer *timer = malloc(sizeof(Timer));
   timer->data = timerData;
   return timer;
}

TimerStatus timer_start(Timer *timer){
   TimerData *timerData = timer->data;  

   if(timerData->started){
      return TimerAlreadyStarted;
   }

   timerData->timeLeftNanos = timerData->config.timeNanos;
   timerData->started = true;

   timerData->next = timers->next;
   timers = timerData;
   return TimerOk;
}

void timer_stop(Timer *timer){
   TimerData *timerData = timer->data;
   timerData->started = false;
}

static void removeTimer(TimerData *timer){
   if(timers == timer){
      timers = timers->next;
   }

   TimerData *prev = timers;
   TimerData *curr = timers->next;
   while(curr){
      if(curr == timer){
         prev->next = curr->next;
         return;
      }
      prev = curr;
      curr = curr->next;
   }
}

void timer_free(Timer *timer){
   TimerData *timerData = timer->data;

   timerData->started = false;
   removeTimer(timerData);

   free(timerData);
   free(timer);
}
