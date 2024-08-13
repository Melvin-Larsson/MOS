#ifndef INCLUDE_KERNEL_IO_H
#define INCLUDE_KERNEL_IO_H

#include "stdarg.h"

typedef enum {
    KIOColorBlack,
    KIOColorBlue,
    KIOColorGreen,
    KIOColorCyan,
    KIOColorRed,
    KIOColorPurple,
    KIOColorBrown,
    KIOColorGray,
    KIOColorDarkGray,
    KIOColorLightBlue,
    KIOColorLightGreen,
    KIOColorLightCyan,
    KIOColorLightRed,
    KIOColorLightPurple,
    KIOColorYellow,
    KIOColorWhite
} KIOColor;

void kio_init();
void kio_setColor(KIOColor color);
KIOColor kio_getColor();
void kprintf(const char* format, ...);
void vkprintf(const char* format, va_list args);
void kprintc(char c, int x, int y);
char kgetc(int x, int y);
void kclear();

#endif
