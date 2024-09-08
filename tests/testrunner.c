#include "testrunner.h"
#include "kernel/kernel-io.h"
#include "stdlib.h"
#include "string.h"
#include "test-list.h"

#define MAX_TEST_COUNT 1000

void printf(const char *, ...);

static void setTestStatus(TestStatus status);
int strcmp(const char *, const char *);

static char *currTestName;
static int currTestLine;
static int testId;
static int ignoredTests;
static TestStatus status[MAX_TEST_COUNT];

static KIOColor prevColor;

static void setColor(KIOColor color){
   switch (color) {
      case KIOColorBlack:
         printf("\033[0;30m");
         break;
      case KIOColorBlue:
         printf("\033[0;34m");
         break;
      case KIOColorGreen:
         printf("\033[0;32m");
         break;
      case KIOColorCyan:
         printf("\033[0;36m");
         break;
      case KIOColorRed:
         printf("\033[0;31m");
         break;
      case KIOColorPurple:
         printf("\033[0;35m");
         break;
      case KIOColorBrown:
         printf("\033[0;33m");
         break;
      case KIOColorGray:
         printf("\033[0;37m");
         break;
      case KIOColorDarkGray:
         printf("\033[1;30m");
         break;
      case KIOColorLightBlue:
         printf("\033[1;34m");
         break;
      case KIOColorLightGreen:
         printf("\033[1;32m");
         break;
      case KIOColorLightCyan:
         printf("\033[1;36m");
         break;
      case KIOColorLightRed:
         printf("\033[1;31m");
         break;
      case KIOColorLightPurple:
         printf("\033[1;35m");
         break;
      case KIOColorYellow:
         printf("\033[1;33m");
         break;
      case KIOColorWhite:
         printf("\033[1;37m");
         break;
      default:
         printf("\033[0m");
         break;
   }
   prevColor = color;
}
static void setErrorColor(){
   setColor(KIOColorLightRed);
}
static void restoreColor(){
   setColor(prevColor);
}

uint32_t thread_getNewEsp(uint32_t esp){ return esp; }

int assertIntL(int actual, int expected, int line){
   if(actual != expected){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, line);
      printf("   AssertInt: Expected %d, got %d\n", expected, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntGTL(int actual, int lowerBound, int line){
   if(actual <= lowerBound){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, line);
      printf("   AssertIntGT: Expected value to be greater than %d, got %d\n", lowerBound, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntGTEL(int actual, int lowerBound, int line){
   if(actual < lowerBound){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, line);
      printf("   AssertIntGE: Expected value to be greater than, or equal to %d, got %d\n", lowerBound, actual);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertIntNotEqualsL(int actual, int notExpected, int line){
   if(actual == notExpected){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, line);
      printf("   AssertIntNotEquals: got %d\n", notExpected);
      setTestStatus(TestStatusFail);
      restoreColor();
      return 0;
   }
   return 1;
}
int assertStringL(char* actual, char* expected, int line){
   if(strcmp(actual, expected) != 0){
      setErrorColor();
      printf("[FAIL] %s (line %d)\n", currTestName, line);
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
void printArrayError(char* actual, uint32_t actualSize, char* expected, uint32_t expectedSize, int line){
         setErrorColor();
         printf("[FAIL] %s (line %d)\n", currTestName, line);
         printf("   AssertArray: Expected " );
         printArray(expected, expectedSize);
         printf(" got ");
         printArray(actual, actualSize);
         printf("\n");
         restoreColor();
}
int assertArrayL(char* actual, uint32_t actualSize, char* expected, uint32_t expectedSize, int line){
   if(actualSize != expectedSize){
      printArrayError(actual, actualSize, expected, expectedSize, line);
      setTestStatus(TestStatusFail);
      return 0;
   }
   for(uint32_t i = 0; i < actualSize; i++){
      if(actual[i] != expected[i]){
         printArrayError(actual, actualSize, expected, expectedSize, line);
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

int main(){
   prevColor = KIOColorWhite;

   testId = -1;
   ignoredTests = 0;
   for(int i = 0; i < MAX_TEST_COUNT; i++){
      status[i] = TestStatusSucess;
   }

//    clear();

   for(int i = 0; i < tests.testCount; i++){
      Test test = tests.tests[i];

      incTestId();
      setTestName(test.name);
         
      if(test.setup){
         test.setup();
      }
      test.test();
      if(test.teardown){
         test.teardown();
      }
   }


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

   printf("Test done! Fails: %d. Successes: %d. Ignored: %d.\n", failed, successfull, ignoredTests);
   restoreColor();

   return failed;
}
