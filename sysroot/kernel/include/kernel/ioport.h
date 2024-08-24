#ifndef IO_PORT_H_INCLUDED
#define IO_PORT_H_INCLUDED

#include "stdint.h"

uint8_t ioport_in8(uint8_t port);
uint16_t ioport_in16(uint8_t port);
uint32_t ioport_in32(uint8_t port);

void ioport_out8(uint8_t port, uint8_t value);
void ioport_out16(uint8_t port, uint16_t value);
void ioport_out32(uint8_t port, uint32_t value);

#endif

