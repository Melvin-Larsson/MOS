#ifndef TEST_RUNNER_H_INCLUDED
#define TEST_RUNNER_H_INCLUDED

#include "stdio.h"
#include "stdint.h"

#define TESTS void runTests(){                        \
   printf("Running %s...\n", __FILE__);               \

#define TEST(Group, Name)\
            incTestId();                              \
            setTestName(#Name);                       \
            setTestLine(__LINE__);                    \
            testSetup_##Group();                      \
            for(int i = 0; i < 1;                     \
               i++ ? testTeardown_##Group() : testTeardown_##Group())\

#define IGNORE_TEST(Group, Name)\
         incIgnoredTests(); \
         if(0) \

#define END_TESTS }

#define TEST_GROUP_SETUP(Group) void testSetup_##Group()

#define TEST_GROUP_TEARDOWN(Group) void testTeardown_##Group()


typedef enum{
   TestStatusSucess,
   TestStatusFail,
}TestStatus;

void runTests();
void kernel_main();

#define assertInt(actual, expected) assertIntL(actual, expected, __LINE__)
int assertIntL(int actual, int expected, int line);

#define assertIntGT(actual, lowerBound) assertIntGTL(actual, lowerBound, __LINE__)
int assertIntGTL(int actual, int lowerBound, int line);

#define assertIntGTE(actual, lowerBound) assertIntGTEL(actual, lowerBound, __LINE__)
int assertIntGTEL(int actual, int lowerBound, int line);

#define assertIntNotEquals(actual, expected) assertIntNotEqualsL(actual, expected, __LINE__)
int assertIntNotEqualsL(int actual, int notExpected, int line);

#define assertString(actual, expected) assertStringL(actual, expected, __LINE__)
int assertStringL(char* actual, char* expected, int line);

int assertArrayL(char* actual, uint32_t actualSize, char* expected, uint32_t expectedSize);

void setTestName(char * name);
void setTestLine(int line);
void incTestId();
void incIgnoredTests();

#endif
