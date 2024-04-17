#include "testrunner.h"
#include "stdio.h"
#include "stdlib.h"
#include "kernel/interrupt.h"
#include "string.h"

#define MAX_TEST_COUNT 1000

static void setTestStatus(TestStatus status);

static char *currTestName;
static int currTestLine;
static int testId;
static int ignoredTests;
static TestStatus status[MAX_TEST_COUNT];

static StdioColor prevColor;
static void setColor(StdioColor color){
   prevColor = stdio_getColor();
   stdio_setColor(color);
}
static void setErrorColor(){
   setColor(StdioColorLightRed);
}
static void restoreColor(){
   stdio_setColor(prevColor);
}

int assertInt(int actual, int expected){
   if(actual != expected){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, currTestLine);
      printf("   AssertInt: Expected %d, got %d\n", expected, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntNotEquals(int actual, int notExpected){
   if(actual == notExpected){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, currTestLine);
      printf("   AssertIntNotEquals: got %d\n", notExpected);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertString(char* actual, char* expected){
   if(!equals(actual, expected)){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, currTestLine);
      printf("   AssertString: Expected %s, got %s\n", expected, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
static void printArray(char *array, uint32_t size){
   printf("[");
   for(uint32_t i = 0; i < size; i++){
      printf("%X", array[i]);
      if(i != size - 1){
         printf(", ");
      }
   }
   printf("]");

}
void printArrayError(char* actual, uint32_t actualSize, char* expected, uint32_t expectedSize){
         setErrorColor();
         printf("[FAIL] %s (line %d)\n", currTestName, currTestLine);
         printf("   AssertArray: Expected " );
         printArray(expected, expectedSize);
         printf(" got ");
         printArray(actual, actualSize);
         printf("\n");
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
   stdioinit();
   stdlib_init();

   interruptDescriptorTableInit();

   testId = -1;
   ignoredTests = 0;
   for(int i = 0; i < MAX_TEST_COUNT; i++){
      status[i] = TestStatusSucess;
   }

   clear();

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
      setColor(StdioColorLightGreen);
   }
   printf("Test done! Fails: %d. Successes: %d. Ignored: %d.\n", failed, successfull, ignoredTests);
   restoreColor();
   while(1);
}
