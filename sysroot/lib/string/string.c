#include "string.h"
#include "stdarg.h"
int strlen(const char* str){
    int length = 0;
    while(*str){
        length++;
        str++;
    }
    return length;
}
char* strcpy(char* destination, const char* source){
    while(*source){
        *destination++ = *source++;
    }
    *destination = 0;
    return destination;
}
void strReadInt(int x, char * output){
    if(x == 0){
        output[0] = '0';
        output[1] = 0;
        return;
    }
    if(x < 0){
        output[0] = '-';
        output++;
        x = -x;
    }
    char buff[10];
    int i = 0;
    for(; x > 0; i++){
        buff[i] = '0' + x % 10; 
        x /= 10;
    }
    for(int j = 0; j < i; j++){
       output[i - j - 1] = buff[j];  
    }
    output[i] = 0;
}
char* strAppend(char *destination, const char* source){
    while(*destination){
        destination++;
    }
    return strcpy(destination, source);
}
char* strAppendFrom(char *destination, const char* source, int start){
    while(start > 0){
        destination++;
        start--;
    }
    return strcpy(destination, source);
}
void sprintf(char *str, const char *format, ...){
    va_list args;
    va_start(args, format);
    
    while(*format){
        if(*format == '%'){
            format++;
            if(*format == 'd'){
                format++;
                char buff[10];
                strReadInt(va_arg(args, int), buff);
                str = strcpy(str, buff);
            }
            if(*format == 's'){
                format++;
                str = strcpy(str, va_arg(args, char *));
            }
            if(*format == 'b'){
                format++;
                if(va_arg(args, int)){
                    char val[6] = "true";
                    str = strcpy(str, val);
                }
                else{
                    char val[6] = "false";
                    str = strcpy(str, val);
                }
            }
        }
        else{
            *str++ = *format++;
        }
    }
    *str = 0;
    va_end(args);
}
