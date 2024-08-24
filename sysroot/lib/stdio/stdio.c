#include "stdio.h"
#include "stdarg.h"

void stdio_setColor(StdioColor newColor){
    __asm__ volatile("int $0x80"
            :
            : "a"(0 << 16 | 1), "b"(newColor));
}

StdioColor stdio_getColor(){
    StdioColor color;
    __asm__ volatile("int $0x80"
            : "=a"(color)
            : "a"(1 << 16 | 1));
    return color;
}

void printf(const char* format, ...){
    va_list args;
    va_start(args, format);
    __asm__ volatile("int $0x80"
            : 
            : "a"(2 << 16 | 1), "b"(format), "c"(args));
    va_end(args);
}

void clear(){
    __asm__ volatile("int $0x80"
            :
            : "a"(3 << 16 | 1));
}
