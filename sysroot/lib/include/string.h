#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

int equals(char *s1, char *s2);
void tolower(char* str);
void toupper(char* str);
int strlen(const char* str);
char* strcpy(char* destination, const char* source);
void strReadInt(int x, char* output);
void strReadIntHex(unsigned int x, char* output);
char* strAppend(char *destination, const char* source);
char* strAppendFrom(char *destination, const char* source, int start);
void sprintf(char *str, const char *format, ...);

#endif
