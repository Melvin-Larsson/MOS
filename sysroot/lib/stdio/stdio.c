#include "stdio.h"
#include "stdarg.h"
#include "stdint.h"
#include "string.h"

#define TERM_WIDTH 80
#define TERM_HEIGHT 25
#define VIDEO_MEMORY ((uint16_t*)0xb8000)


static int x;
static int y;
static StdioColor color;

static void setVMem(uint16_t val, int x, int y);
static uint16_t getVMem(int x, int y);

void stdioinit(){
    x = 0;
    y = 0;
    color = StdioColorWhite;
    clear();
}
void stdio_setColor(StdioColor newColor){
   color = newColor; 
}
StdioColor stdio_getColor(){
    return color;
}
static void rollTerminal(){
    for(int y = 1; y < TERM_HEIGHT; y++){
        for(int x = 0; x < TERM_WIDTH; x++){
            uint16_t c = getVMem(x, y);
            setVMem(c, x, y-1);
        }
    }
    for(int x = 0; x < TERM_WIDTH; x++){
        printc(' ', x, TERM_HEIGHT - 1);
    }
}
static void newLine(){
    y++;
    x = 0;
    if(y >= TERM_HEIGHT){
        y = TERM_HEIGHT - 1;
        rollTerminal();
    }
}
static void printChar(char c){
    if(c == '\n'){
        newLine();
        return;
    }
    if(c == '\b' && x > 0){
        x--;
        printc(' ', x, y);
        return;
    }
    if(c == '\r'){
        return;
    }
    printc(c, x, y);
    x++;
    if(x >= TERM_WIDTH){
        newLine();
    }
}
void printf(const char* str, ...){
    va_list args;
    va_start(args, str);
    
    while(*str){
        if(*str == '%'){
            str++;
            if(*str == 'd'){
                str++;
                char buff[12];
                int nr = va_arg(args, int);
                strReadInt(nr, buff);
                printf(buff);
            }
            if(*str == 's'){
                str++;
                char *str = va_arg(args, char *);
                printf(str);
            }
            if(*str == 'b'){
                str++;
                if(va_arg(args, int)){
                    printf("true");
                }
                else{
                    printf("false");
                }
            }
            if(*str == 'X'){
                str++;
                char buff[32];
                strReadIntHex(va_arg(args, uint32_t), buff);
                printf(buff);
            }
            if(*str == 'c'){
                str++;
                char c = va_arg(args, int);
                printChar(c);
            }
        }
        else{
            printChar(*str);
            str++;
        }
    }
    va_end(args);
}
void printc(char c, int x, int y){
    uint16_t charInfo = (color << 8) | c;
    VIDEO_MEMORY[y * TERM_WIDTH + x] = charInfo;
}
char getc(int x, int y){
    return VIDEO_MEMORY[y * TERM_WIDTH + x];
}
static void setVMem(uint16_t val, int x, int y){
    VIDEO_MEMORY[y * TERM_WIDTH + x] = val;
}
static uint16_t getVMem(int x, int y){
    return VIDEO_MEMORY[y * TERM_WIDTH + x];
}
void clear(){
    for(int y = 0; y < TERM_HEIGHT; y++){
        for(int x = 0; x < TERM_WIDTH; x++){
            VIDEO_MEMORY[y * TERM_WIDTH + x] = 0;
        }    
    }
    x = 0;
    y = 0;
}
