/*  LIB866D
    VGA BIOS & Console Code

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _VGACON_H_
#define _VGACON_H_

#include <stdlib.h>
#include "types.h"

#define VGACON_COLOR_BLACK 0
#define VGACON_COLOR_BLUE  1
#define VGACON_COLOR_GREEN 2
#define VGACON_COLOR_CYAN  3
#define VGACON_COLOR_RED   4
#define VGACON_COLOR_MAGNT 5
#define VGACON_COLOR_BROWN 6
#define VGACON_COLOR_GRAY  7
#define VGACON_COLOR_DGRAY 8
#define VGACON_COLOR_LBLUE 9
#define VGACON_COLOR_LGREN 10
#define VGACON_COLOR_LCYAN 11
#define VGACON_COLOR_LRED  12
#define VGACON_COLOR_LMGNT 13
#define VGACON_COLOR_YELLO 14
#define VGACON_COLOR_WHITE 15

#pragma warning(disable: 4001) /* Non-'int' bitfields in Microsoft C */

#pragma pack(1)

/*  VGA BIOS Character Attribute.
    Foreground and background are VGACON_COLOR_x values.
    Values > COLOR_DGRAY are not available for background due to BLINK bit. */
typedef union {
    struct {
        u16 foreground: 4;
        u16 backgorund: 3;
        u16 blinking: 1;
    } data;
    struct {
        u8 high;
        u8 low;
    } raw;
} vgacon_BIOSColor;

typedef struct {
    u8 x;
    u8 y;
} vgacon_CursorPos;

typedef struct {
    u8 startScanLine;
    u8 endScanLine;
} vgacon_CursorType;

#pragma pack()

/* Prints <length> characters from <str> with <color> character attribute.
   NOTE: This directly addresses VGA text memory, and may fail on Weirdo display types... */
void vgacon_printSizedColorString(const char *str, size_t length, u8 fgColor, u8 bgColor, bool blink);
void vgacon_printColorString(const char *str, u8 fgColor, u8 bgColor, bool blink);
void vgacon_printfColor(u8 fgColor, u8 bgColor, bool blink, const char *fmt, ...);

void vgacon_print       (const char *fmt, ...);
void vgacon_printOK     (const char *fmt, ...);
void vgacon_printWarning(const char *fmt, ...);
void vgacon_printError  (const char *fmt, ...);
void vgacon_printDebug  (const char *fmt, ...);

/* Prints <character> <length> times with color attributes. */
void vgacon_fillColorCharacter(char character, size_t length, u8 fgColor, u8 bgColor, bool blink);
/* Prints <character> <length> times (using existing color attributes) */
void vgacon_fillCharacter(char character, size_t length);

bool vgacon_isCursorAtStartOfLine(void);

/* Get the Width of the text console in characters. */
u16 vgacon_getConsoleWidth(void);
/* Get the Height of the text console in characters. */
u16 vgacon_getConsoleHeight(void);

/* Prints "press any key" message and waits for a key. */
void vgacon_waitKeyWithMessage(void);

#endif
