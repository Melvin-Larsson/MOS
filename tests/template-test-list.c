#include "test-list.h"
#include "stdio.h"

void test(void);

static Test testArray[] = {};

TestArray tests =
   { .tests = testArray,
     .testCount = sizeof(testArray) / sizeof(Test),
     .ignoredTestCount = 0,
   };
