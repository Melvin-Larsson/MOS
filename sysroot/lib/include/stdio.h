#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

void stdioinit();
void printf(const char* str);
void printc(char c, int x, int y);
char getc(int x, int y);
void clear();

#endif
