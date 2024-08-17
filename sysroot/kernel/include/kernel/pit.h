#ifndef PIT_H_INCLUDED
#define PIT_H_INCLUDED

#include "stdint.h"

void pit_init();

void pit_setTimer(void (*)(void *data), void *data, uint32_t time);

#endif
