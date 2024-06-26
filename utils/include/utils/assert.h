#ifndef ASSERT_H_INCLUDED
#define ASSERT_H_INCLUDED

#include "stdio.h"

#ifndef ASSERTS_ENABLED
#define assert(valid) {}
#else
#define assert(valid)                                                                        \
    {                                                                                        \
        if(!(valid)){                                                                        \
            StdioColor oldColor = stdio_getColor();                                          \
            stdio_setColor(StdioColorRed);                                                   \
            printf("Assertion failed in %s at line %d (%s)\n", __FILE__, __LINE__, #valid);  \
            stdio_setColor(oldColor);                                                        \
        }                                                                                    \
    }                                                                           
#endif


#endif
