#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#include "stdarg.h"

int equals(char *s1, char *s2);
void tolower(char* str);
void toupper(char* str);
int strlen(const char* str);
int strContains(char *str, char *seq);
char* strcpy(char* destination, const char* source);
void strReadInt(int x, char* output);
void strReadIntHex(unsigned int x, char* output);
char* strAppend(char *destination, const char* source);
char* strAppendFrom(char *destination, const char* source, int start);
void sprintf(char *str, const char *format, ...);
void vsprintf(char *str, const char *format, va_list args);

#endif
