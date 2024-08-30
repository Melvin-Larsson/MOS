#include "kernel/timer.h"
#include "kernel/pit.h"
#include "collection/list.h"
#include "stdlib.h"

typedef struct TimerData{
   TimerConfig config;
   uint64_t timeLeftNanos;
   bool started;
}TimerData;

static List *timers;

static void pitHandler(void *data, uint16_t cylces);
static uint16_t getPitCycles(TimerData *timer);

void timers_init(){
   pit_init();
   timers = list_newLinkedList(list_pointerEquals);
}
bool timers_freeAll(){
   if(timers->length(timers) != 0){
      return false;
   }

   timers->free(timers);
   return true;
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
      .config = config,
      .timeLeftNanos = config.timeNanos,
      .started = false,
   };

   Timer *timer = malloc(sizeof(Timer));
   timer->data = timerData;
   return timer;
}

static void appendTimerOrdered(List *timerList, TimerData *timer){
   Iterator *iterator = timerList->createIterator(timerList);
   while(iterator->advance(iterator)){
      TimerData *currTimer = iterator->get(iterator);
      if(timer->timeLeftNanos <= currTimer->timeLeftNanos){
         iterator->addAt(iterator, timer);
         iterator->free(iterator);
         return;
      }
   }

   iterator->addAfter(iterator, timer);
   iterator->free(iterator);
}

TimerStatus timer_start(Timer *timer){
   TimerData *timerData = timer->data;  

   if(timerData->started){
      return TimerAlreadyStarted;
   }

   timerData->timeLeftNanos = timerData->config.timeNanos;
   timerData->started = true;

   if(timers->length(timers) > 0){
      pit_stopTimer();
      uint16_t time = pit_cyclesToNanos(pit_getCycles());

      Iterator *iterator = timers->createIterator(timers);
      while(iterator->advance(iterator)){
         TimerData *timer = iterator->get(iterator);
         timer->timeLeftNanos -= time;
      }
      iterator->free(iterator);
   }

   appendTimerOrdered(timers, timerData);

   pit_setTimer(pitHandler, 0, getPitCycles(timers->get(timers, 0)));
   return TimerOk;
}

void timer_stop(Timer *timer){
   TimerData *timerData = timer->data;
   timerData->started = false;
}

void timer_free(Timer *timer){
   TimerData *timerData = timer->data;

   timerData->started = false;
   timers->remove(timers, timerData);

   free(timerData);
   free(timer);
}

static List *removeFinishedTimers(uint64_t passedTime){
   List *result = list_newLinkedList(list_pointerEquals);

   Iterator *iterator = timers->createIterator(timers);
   while(iterator->advance(iterator)){
      TimerData *timer = iterator->get(iterator);

      if(timer->timeLeftNanos > passedTime
            && pit_nanosToCycles(timer->timeLeftNanos - passedTime) > 0){
         timer->timeLeftNanos -= passedTime;
      }
      else{
         timer->timeLeftNanos = 0;
         timer->started = false;

         result->add(result, timer);
         iterator->remove(iterator);
      }
   }

   iterator->free(iterator);

   return result;
}

static void pitHandler(void *data, uint16_t pitCycles){
   uint64_t passedTime = pit_cyclesToNanos(pitCycles);

   List *finishedTimers = removeFinishedTimers(passedTime);
   Iterator *iterator = finishedTimers->createIterator(finishedTimers);

   while(iterator->advance(iterator)){
      TimerData *timer = iterator->get(iterator);

      timer->config.handler(timer->config.data);
      if(timer->config.repeat){
         timer->started = true;
         timer->timeLeftNanos = timer->config.timeNanos;
         appendTimerOrdered(timers, timer);
      }
   }
   iterator->free(iterator);
   finishedTimers->free(finishedTimers);

   if(timers->length(timers) > 0){
//       kprintf("cycles %d\n", getPitCycles(timers->get(timers, 0)));
      pit_setTimer(pitHandler, 0, getPitCycles(timers->get(timers, 0)));
   }
}

static uint16_t getPitCycles(TimerData *timer){
   if(timer == 0){
      return 0;
   }

   uint64_t cycles = pit_nanosToCycles(timer->timeLeftNanos);
   return cycles > 0xFFFF ? 0xFFFF : cycles;
}
