#ifndef PIT_H_INCLUDED
#define PIT_H_INCLUDED

#include "stdint.h"

void pit_init();

void pit_setTimer(void (*)(void *data, uint16_t), void *data, uint32_t pitCycles);
void pit_stopTimer();
uint16_t pit_getCycles();

uint64_t pit_nanosToCycles(uint64_t nanos);
uint64_t pit_cyclesToNanos(uint64_t cycles);

#endif
