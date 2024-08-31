#include "kernel/kernel-io.h"
#include "kernel/interrupt.h"
#include "kernel/memory.h"
#include "testrunner.h"
#include "stdlib.h"
#include "string.h"

#define MAX_TEST_COUNT 1000

static void setTestStatus(TestStatus status);

static char *currTestName;
static int currTestLine;
static int testId;
static int ignoredTests;
static TestStatus status[MAX_TEST_COUNT];

static KIOColor prevColor;
static void setColor(KIOColor color){
   prevColor = stdio_getColor();
   kio_setColor(color);
}
static void setErrorColor(){
   setColor(KIOColorLightRed);
}
static void restoreColor(){
   kio_setColor(prevColor);
}

uint32_t thread_getNewEsp(uint32_t esp){ return esp; }

int assertIntL(int actual, int expected, int line){
   if(actual != expected){
      setErrorColor();
      kprintf("[FAIL] %s (line %d)\n", currTestName, line);
      kprintf("   AssertInt: Expected %d, got %d\n", expected, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntGTL(int actual, int lowerBound, int line){
   if(actual <= lowerBound){
      setErrorColor();
      kprintf("[FAIL] %s (line %d)\n", currTestName, line);
      kprintf("   AssertIntGT: Expected value to be greater than %d, got %d\n", lowerBound, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntGTEL(int actual, int lowerBound, int line){
   if(actual < lowerBound){
      setErrorColor();
      kprintf("[FAIL] %s (line %d)\n", currTestName, line);
      kprintf("   AssertIntGE: Expected value to be greater than, or equal to %d, got %d\n", lowerBound, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntNotEqualsL(int actual, int notExpected, int line){
   if(actual == notExpected){
      setErrorColor();
      kprintf("[FAIL] %s (line %d)\n", currTestName, line);
      kprintf("   AssertIntNotEquals: got %d\n", notExpected);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertStringL(char* actual, char* expected, int line){
   if(!equals(actual, expected)){
      setErrorColor();
      kprintf("[FAIL] %s (line %d)\n", currTestName, line);
      kprintf("   AssertString: Expected %s, got %s\n", expected, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
static void printArray(char *array, uint32_t size){
   kprintf("[");
   for(uint32_t i = 0; i < size; i++){
      kprintf("%X", array[i]);
      if(i != size - 1){
         kprintf(", ");
      }
   }
   kprintf("]");

}
void printArrayError(char* actual, uint32_t actualSize, char* expected, uint32_t expectedSize){
         setErrorColor();
         kprintf("[FAIL] %s (line %d)\n", currTestName, currTestLine);
         kprintf("   AssertArray: Expected " );
         printArray(expected, expectedSize);
         kprintf(" got ");
         printArray(actual, actualSize);
         kprintf("\n");
         restoreColor();
}
int assertArray(char* actual, uint32_t actualSize, char* expected, uint32_t expectedSize){
   if(actualSize != expectedSize){
      printArrayError(actual, actualSize, expected, expectedSize);
      setTestStatus(TestStatusFail);
      return 0;
   }
   for(uint32_t i = 0; i < actualSize; i++){
      if(actual[i] != expected[i]){
         printArrayError(actual, actualSize, expected, expectedSize);
         setTestStatus(TestStatusFail);
         return 0;
      }
   }
   return 1;
}

void setTestName(char * name){
   currTestName = name;
}
void setTestLine(int line){
   currTestLine = line;
}
void incTestId(){
   testId++;
}
void incIgnoredTests(){
   ignoredTests++;
}
static void setTestStatus(TestStatus s){
   if(status[testId] == TestStatusSucess){
      status[testId] = s;
   }
}

void kernel_main(){
   kio_init();
   memory_init();

   interruptDescriptorTableInit();

   testId = -1;
   ignoredTests = 0;
   for(int i = 0; i < MAX_TEST_COUNT; i++){
      status[i] = TestStatusSucess;
   }

   clear();

   debug_logMemory();

   runTests();

   uint32_t successfull = 0;
   uint32_t failed = 0;
   for(int i = 0; i <= testId; i++){
      if(status[i] == TestStatusSucess){
         successfull++;
      }
      else if(status[i] == TestStatusFail){
         failed++;
      }
   }
   if(failed > 0){
      setErrorColor();
   }else{
      setColor(KIOColorLightGreen);
   }
   kprintf("Test done! Fails: %d. Successes: %d. Ignored: %d.\n", failed, successfull, ignoredTests);
   debug_logMemory();
   restoreColor();
   while(1);
}
