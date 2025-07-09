/*  LIB866D
    VGA BIOS & Console Code

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _VGACON_H_
#define _VGACON_H_

#include <stdlib.h>
#include <stdarg.h>

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

/*  Printf-style logging functions with colorized type text (except print which is blank) at
    column 6 - these are associated with a logging level that can be set. */
void vgacon_printDebug  (const char *fmt, ...); /* Requires VGACON_LOG_LEVEL_DEBUG (muted by default for non-debug builds!!!) */
void vgacon_print       (const char *fmt, ...); /* Requires VGACON_LOG_LEVEL_INFO or below */
void vgacon_printOK     (const char *fmt, ...); /* Requires VGACON_LOG_LEVEL_OK or below */
void vgacon_printWarning(const char *fmt, ...); /* Requires VGACON_LOG_LEVEL_WARNING or below */
void vgacon_printError  (const char *fmt, ...); /* Requires VGACON_LOG_LEVEL_ERROR or below */

/* VGA Console logging Level for enabling quiet modes */
typedef enum {
    VGACON_LOG_LEVEL_DEBUG      = 0,    /* printDebug and above will display (i.e. everything) (DEFAULT for debug builds) */
    VGACON_LOG_LEVEL_INFO       = 1,    /* print and above will display (DEFAULT for non-debug builds) */
    VGACON_LOG_LEVEL_OK         = 2,    /* printOK and above will display */
    VGACON_LOG_LEVEL_WARNING    = 3,    /* printWarning and above will display*/
    VGACON_LOG_LEVEL_ERROR      = 4,    /* printError will display */
    VGACON_LOG_LEVEL_SILENT     = 5,    /* Nothing will display */
} vgacon_LogLevel;

/* Sets the logging level, below which print<n> functions will not display anyhting.
    Default is VGACON_LOG_LEVEL_INFO */
void vgacon_setLogLevel(vgacon_LogLevel level);
/* vprintf-esque function to print at a given log level. Optionally prints a new line if newLine is set. */
void vgacon_vprintfLogLevel(vgacon_LogLevel level, const char *fmt, va_list args, bool newLine);

/* Prints <character> <length> times with color attributes. */
void vgacon_fillColorCharacter(char character, size_t length, u8 fgColor, u8 bgColor, bool blink);
/* Prints <character> <length> times (using existing color attributes) */
void vgacon_fillCharacter(char character, size_t length);
/* Returns true if the cursor is on the first column of the current line. */
bool vgacon_isCursorAtStartOfLine(void);

/* Get the Width of the text console in characters. */
u16 vgacon_getConsoleWidth(void);
/* Get the Height of the text console in characters. */
u16 vgacon_getConsoleHeight(void);

/* Prints "press any key" message and waits for a key. */
void vgacon_waitKeyWithMessage(void);

#endif
