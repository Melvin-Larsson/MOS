#include "test-list.h"
#include "stdio.h"

void test(void);

static Test testArray[] = {};

TestArray tests = {testArray, sizeof(testArray) / sizeof(Test)};
