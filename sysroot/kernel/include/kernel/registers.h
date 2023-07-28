#ifndef REGISTERS_H_INCLUDED
#define REGISTERS_H_INCLUDED

#include "stdint.h"
typedef struct{
    union{
         uint32_t ss;
         struct{
            char privilige : 2;
            uint32_t rest: 30;
         };
    };
}SS;

SS registersGetSS();

#endif
