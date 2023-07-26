#include "stdio.h"

#include "stdint.h"
#define TERM_WIDTH 80
#define TERM_HEIGHT 25
#define VIDEO_MEMORY ((uint16_t*)0xb8000)


static int x = 0;
static int y = 0;
void stdioinit(){
    x = 0;
    y = 0;
    clear();
}
static void rollTerminal(){
    for(int y = 1; y < TERM_HEIGHT; y++){
        for(int x = 0; x < TERM_WIDTH; x++){
            char c = getc(x, y);
            printc(c, x, y-1);
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
    printc(c, x, y);
    x++;
    if(x >= TERM_WIDTH){
        newLine();
    }
}
void printf(const char* str){
    while(*str){
        printChar(*str);
        str++;
    }
}
void printc(char c, int x, int y){
    uint16_t charInfo = 0x0F00 | c;
    VIDEO_MEMORY[y * TERM_WIDTH + x] = charInfo;
}
char getc(int x, int y){
    return VIDEO_MEMORY[y * TERM_WIDTH + x];
}
void clear(){
    for(int y = 0; y < TERM_HEIGHT; y++){
        for(int x = 0; x < TERM_WIDTH; x++){
            VIDEO_MEMORY[y * TERM_WIDTH + x] = 0;
        }    
    }
}
