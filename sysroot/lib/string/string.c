#include "string.h"

int equals(char *s1, char *s2){
   while(*s1 && *s2){
      if(*s1 != *s2){
         return 0;
      }
      s1++;
      s2++;
   }
   return *s1 == *s2;
}

void tolower(char* str){
    while(*str){
        if(*str >= 'A' && *str <= 'Z'){
            *str += 'a' - 'A';
        }
        str++;
    }
}
void toupper(char* str){
    while(*str){
        if(*str >= 'a' && *str <= 'z'){
            *str += 'A' - 'a';
        }
        str++;
    }
}
int strContains(char *str, char *seq){
    if(*seq == 0){
        return 1;
    }
    while(*str){
        int i = 0;
        for(; seq[i]; i++){
            if(str[i] == 0){
                return 0;
            }
            if(seq[i] != str[i]){
                break;
            }
        }
        if(seq[i] == 0){
            return 1;
        }
        str++;
    }
    return 0;
}
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
    char buff[12];
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
void strReadIntHex(unsigned int x, char* output){
    output = strcpy(output, "0x");
    if(x == 0){
        strcpy(output, "0");
        return;
    }
    
    char buff[32];
    int i = 0;
    for(; x; i++){
        char quad = x & 0xF;
        if(quad >= 10){
            buff[i] = 'A' + (quad - 10);
        }else{
            buff[i] = '0' + quad;
        }
        x >>= 4;
    }
    for(int j = 0; j < i; j++){
        output[i - 1 - j] = buff[j];
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
char* sprintf(char *str, const char *format, ...){
    va_list args;
    va_start(args, format);
    char *res = vsprintf(str, format, args);
    va_end(args);
    return res;
}
char* vsprintf(char *str, const char *format, va_list args){
    while(*format){
        if(*format == '%'){
            format++;
            if(*format == 'd'){
                format++;
                char buff[10];
                strReadInt(va_arg(args, int), buff);
                str = strcpy(str, buff);
            }
            else if(*format == 's'){
                format++;
                str = strcpy(str, va_arg(args, char *));
            }
            else if(*format == 'b'){
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
            else if(*format == 'X'){
                format++;
                char buff[32];
                strReadIntHex(va_arg(args, int), buff);
                str = strcpy(str, buff);
            }
            else if(*format == 'c'){
                format++;
                char c = va_arg(args, int);
                *str++ = c;
            }
        }
        else{
            *str++ = *format++;
        }
    }
    *str = 0;
    return str;
}
