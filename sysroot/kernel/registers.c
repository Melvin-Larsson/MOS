
#include "kernel/registers.h"
SS registersGetSS(){
   SS ss;
   __asm__ volatile("mov %%ss, %0" : "=r"(ss));
   return ss;
}
