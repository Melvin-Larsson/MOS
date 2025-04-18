#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

typedef enum {
    StdioColorBlack,
    StdioColorBlue,
    StdioColorGreen,
    StdioColorCyan,
    StdioColorRed,
    StdioColorPurple,
    StdioColorBrown,
    StdioColorGray,
    StdioColorDarkGray,
    StdioColorLightBlue,
    StdioColorLightGreen,
    StdioColorLightCyan,
    StdioColorLightRed,
    StdioColorLightPurple,
    StdioColorYellow,
    StdioColorWhite
} StdioColor;


void stdio_setColor(StdioColor color);
StdioColor stdio_getColor();
void printf(const char* format, ...);
void clear();

#endif
