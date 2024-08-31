#include "testrunner.h"
#include "kernel/timer.h"
#include "kernel/pit.h"
#include "stdlib.h"

static void (*pitHandler)(void *data, uint16_t time);
static void *pitData;
static uint32_t pitTotalTime;
static uint32_t pitTimersStarted;
static uint32_t pitTimersStoped;

static uint16_t currPitTime;

void pit_init(){}
void pit_setDirectTimer(void (*handler)(void), uint32_t pitCycles){}
void pit_checkoutInterrupt(){}
void pit_setTimer(void (*handler)(void *, uint16_t), void *data, uint32_t time){
   pitHandler = handler;
   pitData = data;
   pitTotalTime = time;
   pitTimersStarted++;
   assertIntGTE(pitTimersStoped, pitTimersStoped);
   currPitTime = 0;
}
void pit_stopTimer(){
   pitTimersStoped++;
}
uint16_t pit_getCycles(){
   return currPitTime;
}
uint64_t pit_nanosToCycles(uint64_t nanos){
   return nanos;
}
uint64_t pit_cyclesToNanos(uint64_t cycles){
   return cycles;
}

static int handledData[128];
static int dataCount;
static void handler(void *data){
   handledData[dataCount++] = (int)data;
}

static bool handledDataContains(int value, unsigned int count){
   unsigned int foundCount = 0;
   for(int i = 0; i < dataCount; i++){
      if(handledData[i] == value){
         foundCount++;
      }
   }
   return foundCount == count;
}

static Timer *timers[128];
static int timerCount;
static Timer *createTimer(int data, uint64_t timeNanos){
   TimerConfig tc = timer_createDefaultConfig(handler, (void*)data, timeNanos);
   Timer *timer = timer_new(tc);
   timers[timerCount++] = timer;
   return timer;
}
static Timer *createRepeatTimer(int data, uint64_t timeNanos){
   TimerConfig tc = timer_createDefaultConfig(handler, (void*)data, timeNanos);
   tc.repeat = true;
   Timer *timer = timer_new(tc);
   timers[timerCount++] = timer;
   return timer;
}

static void setCurrPitTime(uint16_t pitTime){
   currPitTime = pitTime;

   if(pitHandler && currPitTime >= pitTotalTime){
      pitHandler(pitData, currPitTime);
      currPitTime = 0;
   }
}

TEST_GROUP_SETUP(dtg){
   timers_init();

   pitHandler = 0;
   pitData = 0;
   pitTotalTime = 0;
   pitTimersStarted = 0;
   pitTimersStoped = 0;
   currPitTime = 0;

   memset(timers, 0, sizeof(timers));
   timerCount = 0;

   memset(handledData, 0, sizeof(handledData));
   dataCount = 0;
}
TEST_GROUP_TEARDOWN(dtg){
   for(int i = 0; i < timerCount; i++){
      timer_free(timers[i]);
   }
   assertInt(timers_freeAll(), true);;
}

TESTS

TEST(dtg, singleTimer1000ns_pitTimerStarted){
   Timer *timer = createTimer(1, 1000);
   TimerStatus status = timer_start(timer);

   assertInt(status, TimerOk);
   assertInt(pitTotalTime, 1000);
   assertInt(pitTimersStarted, 1);
}

TEST(dtg, singleTimer0xFFFFns_pitTimerStarted){
   Timer *timer = createTimer(1, 0xFFFF);
   TimerStatus status = timer_start(timer);

   assertInt(status, TimerOk);
   assertInt(pitTotalTime, 0xFFFF);
   assertInt(pitTimersStarted, 1);
}

TEST(dtg, timerStartedSecondTimeBeforeFinishing_timerAlreadyStartedError){
   Timer *timer = createTimer(1, 1000);
   timer_start(timer);

   TimerStatus status = timer_start(timer);

   assertInt(status, TimerAlreadyStarted);
}

TEST(dtg, timerStartedSecondTimeAfterFinishing_returnsOk_handlerCalledTwice){
   uint32_t time = 1000;

   Timer *timer = createTimer(1, time);
   timer_start(timer);

   setCurrPitTime(time);
   TimerStatus status = timer_start(timer);
   setCurrPitTime(time);

   assertInt(status, TimerOk);
   assertInt(dataCount, 2);
}

IGNORE_TEST(dtg, singleTimer0ns_noPitTimerStarted){
   Timer *timer = createTimer(1, 0);
   TimerStatus status = timer_start(timer);

   assertInt(status, TimerOk);
   assertInt(pitTimersStarted, 0);
}

TEST(dtg, singeTimerDone_handlerCalledWithData1){
   int data = 1;
   uint32_t time = 1000;

   Timer *timer = createTimer(data, time);
   timer_start(timer);

   setCurrPitTime(time);
   assertInt(dataCount, 1);
   assertInt(handledData[0], data);
}

TEST(dtg, singeTimerDone_handlerCalledWithData0x69){
   int data = 0x69;
   uint32_t time = 1000;

   Timer *timer = createTimer(data, time);
   timer_start(timer);

   setCurrPitTime(time);
   assertInt(dataCount, 1);
   assertInt(handledData[0], data);
}

TEST(dtg, singleTimerDoneButTookLongerThanExpected_handlerCalled){
   uint32_t time = 1000;

   Timer *timer = createTimer(1, time);
   timer_start(timer);

   setCurrPitTime(3000);

   assertInt(dataCount, 1);
}

TEST(dtg, singleTimerLongerTimeThanPitMax_handlerCalled){
   int data = 0x69;
   uint32_t time = 0xFFFF * 3 + 1000;

   Timer *timer = createTimer(data, time);
   timer_start(timer);

   for(uint32_t i = 0; i < time / 0xFFFF; i++){
      assertInt(pitTotalTime, 0xFFFF);
      setCurrPitTime(0xFFFF);
   }
   assertInt(pitTotalTime, 1000);
   setCurrPitTime(1000);
   assertInt(dataCount, 1);
}

TEST(dtg, repeatTimerDone_handlerCalledRepeatedly){
   uint32_t time = 1000;
   int data = 0x69;

   Timer *timer = createRepeatTimer(data, time);
   timer_start(timer);

   bool success = true;
   for(int i = 0; i < 64 && success; i++){
      success &= assertInt(pitTimersStarted, i + 1);
      setCurrPitTime(time);
      success &= assertInt(dataCount, i + 1);
      success &= assertInt(handledData[i], data);
   }
}
TEST(dtg, twoTimersDone_twoHandlersCalledWithData_singlePitTimerStarted){
   int data1 = 1;
   int data2 = 2;
   uint32_t time = 1000;

   Timer *timer1 = createTimer(data1, time);
   Timer *timer2 = createTimer(data2, time);

   timer_start(timer1);
   timer_start(timer2);

   setCurrPitTime(time);

   assertInt(handledDataContains(data1, 1), true);
   assertInt(handledDataContains(data2, 1), true);
}

TEST(dtg, twoRepeatTimerDone_twoHandlersCalledWithData){
   int data1 = 1;
   int data2 = 2;
   uint32_t time = 1000;

   Timer *timer1 = createRepeatTimer(data1, time);
   Timer *timer2 = createRepeatTimer(data2, time);

   timer_start(timer1);
   timer_start(timer2);

   for(int i = 0; i < 32; i++){
      setCurrPitTime(time);

      assertInt(handledDataContains(data1, i + 1), true);
      assertInt(handledDataContains(data2, i + 1), true);
   }
}

TEST(dtg, 128TimersDone_128HandlersCalledWithData){
   uint32_t time = 1000;
   Timer *timers[128];
   for(int i = 0; i < 128; i++){
      timers[i] = createTimer(i, time);
      timer_start(timers[i]);
   }

   setCurrPitTime(time);

   for(int i = 0; i < 128; i++){
      assertInt(handledDataContains(i, 1), true);
   }
}

TEST(dtg, secondShorterTimerStarted_pitTimerShortTime){
   uint32_t longTime = 5000, shortTime = 1000;
   Timer* longTimer = createTimer(1, longTime);
   Timer* shortTimer = createTimer(1, shortTime);

   timer_start(longTimer);
   timer_start(shortTimer);

   assertInt(pitTotalTime, shortTime);
}

TEST(dtg, secondLongerTimerStarted_pitTimerShortTime){
   uint32_t longTime = 5000, shortTime = 1000;
   Timer* longTimer = createTimer(1, longTime);
   Timer* shortTimer = createTimer(1, shortTime);

   timer_start(shortTimer);
   timer_start(longTimer);

   assertInt(pitTotalTime, shortTime);
}

TEST(dtg, 3TimersDoneWithDifferentTimes_3HandlersCalled_3PitTimersStarted){
   int data[] = {1,2,3};
   uint32_t times[] = {3000, 2000, 1000};
   Timer *timers[3];
   for(int i = 0; i < 3; i++){
      timers[i] = createTimer(data[i], times[i]);
      timer_start(timers[i]);
   }

   for(int i = 0; i < 3; i++){
      assertInt(pitTotalTime, 1000);

      setCurrPitTime(1000);
      assertInt(handledData[i], data[2 - i]);
   }

   assertInt(dataCount, 3);
}

TEST(dtg, usingRepeatTimers_firstShorterTimerTriggered_withSecondShortlyAfter_pitSetToTimeDiff){
   uint32_t time1 = 1000, time2 = 1200;
   int data1 = 1, data2 = 2;

   Timer *timer1 = createRepeatTimer(data1, time1);
   Timer *timer2 = createRepeatTimer(data2, time2);

   timer_start(timer1);
   timer_start(timer2);

   setCurrPitTime(time1);
   assertInt(pitTotalTime, 200);
}



END_TESTS
