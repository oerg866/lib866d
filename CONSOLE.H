/*  LIB866D
    Text Console Output

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "types.h"
#include <stdlib.h>

#define CON_COLOR_BLACK     0
#define CON_COLOR_BLUE      1
#define CON_COLOR_GREEN     2
#define CON_COLOR_CYAN      3
#define CON_COLOR_RED       4
#define CON_COLOR_MAGENTA   5
#define CON_COLOR_BROWN     6
#define CON_COLOR_GRAY      7
#define CON_COLOR_DGRAY     8
#define CON_COLOR_LBLUE     9
#define CON_COLOR_LGREEN    10
#define CON_COLOR_LCYAN     11
#define CON_COLOR_LRED      12
#define CON_COLOR_LMAGENTA  13
#define CON_COLOR_YELLOW    14
#define CON_COLOR_WHITE     15

/*  Usable as either half of a color pair: keep whatever the shell was using
    when the program started. The console samples that once, from the cell
    under the cursor, and puts it back on a fresh line as the program ends. */
#define CON_COLOR_DEFAULT   0xFF

/*  Messages below the configured level are discarded. */
typedef enum {
    con_levelDebug = 0,     /* con_debug and above (everything)  */
    con_levelInfo,          /* con_print and above (the default) */
    con_levelOk,            /* con_ok and above                  */
    con_levelWarning,       /* con_warning and above             */
    con_levelError,         /* con_error only                    */
    con_levelSilent         /* nothing at all                    */
} con_Level;

typedef struct {
    u8 x;
    u8 y;
} con_Position;

/*  Set the minimum level a message must have to be displayed. */
void con_setLevel           (con_Level level);
/*  Get the currently configured minimum level. */
con_Level con_getLevel      (void);
/*  Get current horizontal size of the console */
u16 con_getColumns(void);
/*  Get current vertical size of the console */
u8 con_getRows(void);
/*  Get the current cursor position */
con_Position con_getCursor  (void);

/*  Print <text> in the given colors at the cursor, without a trailing newline.
    Either color may be CON_COLOR_DEFAULT to inherit the shell's.
    Scrolls the screen if it runs past the bottom. Not affected by the level. */
void con_colorText          (const char *text, u8 foreground, u8 background);
void con_colorTextSized     (const char *text, size_t length, u8 foreground, u8 background);

/*  Print the character <toFill> <length>-times at the cursor.
    Either color may be CON_COLOR_DEFAULT to inherit the shell's.
    Scrolls the screen if it runs past the bottom. Not affected by the level. */
void con_colorFill          (char toFill, size_t length, u8 foreground, u8 background);

/*  printf-style output. Each of these is gated on the configured level, and
    all but con_print prefix the line with a colored tag. con_print still
    reserves the status gutter so that its lines stay aligned with the rest. */
void con_debug              (const char *fmt, ...);
void con_print              (const char *fmt, ...);

/*  printf-style output with no status gutter at all, for text that wants the
    whole width of the screen, such as the usage screen.
    This is what you should use instead of printf if you use the con_ library. */
void con_plain              (const char *fmt, ...);

/*  Whether output is still going to a screen rather than to a file or a
    pipe. Anything that wants to draw for itself has to ask first. */
bool con_ownsScreen         (void);
void con_ok                 (const char *fmt, ...);

void con_warning            (const char *fmt, ...);
void con_error              (const char *fmt, ...);

/*  Put a singular character on screen. */
void con_putc               (char c);

#endif

