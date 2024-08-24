#include "kernel/ioport.h"

uint8_t ioport_in8(uint8_t port){
   uint8_t result;
   __asm__ volatile("in %%dx, %%al"
         : "=a"(result)
         : "d"(port));
   return result;
}
uint16_t ioport_in16(uint8_t port){
   uint8_t result;
   __asm__ volatile("in %%dx, %%ax"
         : "=a"(result)
         : "d"(port));
   return result;
}
uint32_t ioport_in32(uint8_t port){
   uint8_t result;
   __asm__ volatile("in %%dx, %%eax"
         : "=a"(result)
         : "d"(port));
   return result;
}

void ioport_out8(uint8_t port, uint8_t value){
   __asm__ volatile("out %%al, %%dx"
         : 
         : "a"(value), "d"(port));
}
void ioport_out16(uint8_t port, uint16_t value){
   __asm__ volatile("out %%ax, %%dx"
         : 
         : "a"(value), "d"(port));
}
void ioport_out32(uint8_t port, uint32_t value){
   __asm__ volatile("out %%eax, %%dx"
         : 
         : "a"(value), "d"(port));
}
