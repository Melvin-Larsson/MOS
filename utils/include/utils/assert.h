#ifndef ASSERT_H_INCLUDED
#define ASSERT_H_INCLUDED

#include "kernel/logging.h"

static inline int assert_impl(int valid, char *file, int line, char *expression){
    if(!(valid)){
        loggError("Assertion failed in %s at line %d (%s)\n", file, line, expression);
        while(1);
    }
    return valid;
}

#ifndef ASSERTS_ENABLED
#define assert(valid) 1
#else
#define assert(valid) assert_impl(valid, __FILE__, __LINE__, #valid)
#endif


#endif
