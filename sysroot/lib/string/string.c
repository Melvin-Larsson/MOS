#include "string.h"
#include <stdint.h>

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
//FIXME: should return destination as is from the beginning
char* strcpy(char* destination, const char* source){
    while(*source){
        *destination++ = *source++;
    }
    *destination = 0;
    return destination;
}
char *strncpy(char *destination, const char *source, size_t num){
    while(*source && num > 0){
        *destination++ = *source++;
        num--;
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
int sprintf(char *str, const char *format, ...){
    va_list args;
    va_start(args, format);
    int res = vsprintf(str, format, args);
    va_end(args);
    return res;
}
int vsprintf(char *str, const char *format, va_list args){
    return vsnprintf(str, SIZE_MAX, format, args);
}

int snprintf(char *str, size_t size, const char *format, ...){
    va_list args;
    va_start(args, format);
    int res = vsnprintf(str, size, format, args);
    va_end(args);
    return res;
}

static int getCopyLength(char *str, size_t used, size_t total){
    if(used >= total){
        return 0;
    }
    int len = strlen(str);
    if(used + len <= total){
        return len;
    }
    return total - used;
}
int vsnprintf(char *str, size_t size, const char *format, va_list args){
    size_t used = 0;
    while(*format){
        if(*format == '%'){
            format++;
            if(*format == 'd'){
                format++;
                char buff[10];
                strReadInt(va_arg(args, int), buff);
                str = strncpy(str, buff, getCopyLength(buff, used, size));
                used += strlen(buff);
            }
            else if(*format == 's'){
                format++;
                char *s = va_arg(args, char *);
                str = strncpy(str, s, getCopyLength(s, used, size));
                used += strlen(s);
            }
            else if(*format == 'b'){
                format++;
                if(va_arg(args, int)){
                    char val[6] = "true";
                    str = strncpy(str, val, getCopyLength(val, used, size));
                    used += strlen(val);
                }
                else{
                    char val[6] = "false";
                    str = strncpy(str, val, getCopyLength(val, used, size));
                    used += strlen(val);
                }
            }
            else if(*format == 'X'){
                format++;
                char buff[32];
                strReadIntHex(va_arg(args, int), buff);
                str = strncpy(str, buff, getCopyLength(buff, used, size));
                used += strlen(buff);
            }
            else if(*format == 'c'){
                format++;
                char c = va_arg(args, int);
                used += 1;
                if(used <= size){
                    *str++ = c;
                }
            }
        }
        else{
            used++;
            char c = *format++;
            if(used <= size){
                *str++ = c;
            }
        }
    }
    *str = 0;
    return used;
}
