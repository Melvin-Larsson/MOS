#ifndef ASSERT_H_INCLUDED
#define ASSERT_H_INCLUDED

#include "stdio.h"

static int assert_impl(int valid, char *file, int line, char *expression){
    if(!(valid)){
        StdioColor oldColor = stdio_getColor();
        stdio_setColor(StdioColorRed);
        printf("Assertion failed in %s at line %d (%s)\n", file, line, expression);
        stdio_setColor(oldColor);
    }
    return valid;
}

#ifndef ASSERTS_ENABLED
#define assert(valid) 1
#else
#define assert(valid) assert_impl(valid, __FILE__, __LINE__, #valid)
#endif


#endif
