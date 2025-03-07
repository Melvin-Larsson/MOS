#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#include "stdarg.h"
#include "stddef.h"

int equals(char *s1, char *s2);
void tolower(char* str);
void toupper(char* str);
int strlen(const char* str);
int strContains(char *str, char *seq);
char* strcpy(char* destination, const char* source);
char* strncpy(char* destination, const char* source, size_t num);
void strReadInt(int x, char* output);
void strReadIntHex(unsigned int x, char* output);
char* strAppend(char *destination, const char* source);
char* strAppendFrom(char *destination, const char* source, int start);

int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsprintf(char *str, const char *format, va_list args);
int vsnprintf(char *str, size_t size, const char *format, va_list args);

#endif
