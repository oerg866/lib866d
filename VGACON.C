/*  LIB866D
    VGA BIOS & Console Code

    (C) 2024 E. Voirin (oerg866)
 */

/* 	AH = 13h
	AL = write mode (see bit settings below)
	   = 0 string is chars only, attribute in BL, cursor not moved
	   = 1 string is chard only, attribute in BL, cursor moved
	   = 2 string contains chars and attributes, cursor not moved
	   = 3 string contains chars and attributes, cursor moved
	BH = video page number
	BL = attribute if mode 0 or 1 (AL bit 1=0)
	CX = length of string (ignoring attributes)
	DH = row coordinate
	DL = column coordinate
	ES:BP = pointer to string*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "386asm.h"
#include "vgacon.h"
#include "conio.h"
#include "util.h"

#define VGACON_MAKE_COLOR(fg, bg, blink) ((u8)((((u8) bgColor & 0x07) << 4) | ((u8) fgColor & 0x0f) | ((u8) blink << 7)))

typedef struct {
    char    c;      /* character */
    u8      attr;   /* VGA color attribute */
} vgacon_BIOSChar;

u8                  _far *vgacon_MEM_CurrentVideoMode   = MK_FP(0x0040, 0x0049);
u16                 _far *vgacon_MEM_ColumnsOnScreen    = MK_FP(0x0040, 0x004A);
u16                 _far *vgacon_MEM_PageSizeBytes      = MK_FP(0x0040, 0x004C);
u16                 _far *vgacon_MEM_CurrentPageOffset  = MK_FP(0x0040, 0x004E);
vgacon_CursorPos    _far *vgacon_MEM_CursorPositions    = MK_FP(0x0040, 0x0050);
vgacon_CursorType   _far *vgacon_MEM_CurrentCusorType   = MK_FP(0x0040, 0x0060);
u8                  _far *vgacon_MEM_CurrentPageNumber  = MK_FP(0x0040, 0x0062);
u8                  _far *vgacon_MEM_HighestColumnIndex = MK_FP(0x0040, 0x0084);
u8                  _far *vgacon_MEM_VideoRAM           = MK_FP(0xB800, 0x0000);

static void vgacon_advanceCursor(size_t increment) {
    u16 width = *vgacon_MEM_ColumnsOnScreen;
    u16 newx = vgacon_MEM_CursorPositions[*vgacon_MEM_CurrentPageNumber].x;
    newx += (u16) increment;
    vgacon_MEM_CursorPositions[*vgacon_MEM_CurrentPageNumber].y += (u8) (newx / width);
    vgacon_MEM_CursorPositions[*vgacon_MEM_CurrentPageNumber].x = (u8) newx;
}

#define VGACON_CURRENT_CURSOR_POSITION (vgacon_MEM_CursorPositions[*vgacon_MEM_CurrentPageNumber])
#define VGACON_CURRENT_DRAW_POINTER() \
     (((vgacon_BIOSChar _far *) &vgacon_MEM_VideoRAM[*vgacon_MEM_CurrentPageOffset]) \
     + (vgacon_MEM_CursorPositions[*vgacon_MEM_CurrentPageNumber].y * *vgacon_MEM_ColumnsOnScreen) \
     + (vgacon_MEM_CursorPositions[*vgacon_MEM_CurrentPageNumber].x))

static vgacon_BIOSChar _far *vgacon_getCurrentDrawPointer(void) {
    vgacon_BIOSChar _far *drawPtr = (vgacon_BIOSChar _far *) &vgacon_MEM_VideoRAM[*vgacon_MEM_CurrentPageOffset];
    vgacon_CursorPos pos;
    fflush(stdout);
    pos = VGACON_CURRENT_CURSOR_POSITION;
    drawPtr += pos.y * *vgacon_MEM_ColumnsOnScreen;
    drawPtr += pos.x;
    return drawPtr;
}

void vgacon_printSizedColorString(const char *str, size_t length, u8 fgColor, u8 bgColor, bool blink) {
    vgacon_BIOSChar current;
    vgacon_BIOSChar _far* drawPtr = vgacon_getCurrentDrawPointer();

    current.attr = VGACON_MAKE_COLOR(fgColor, bgColor, blink);

    vgacon_advanceCursor(length);

    while (length--) {
        current.c = *str++;
        *drawPtr++ = current;
    }
}

void vgacon_printColorString(const char *str, u8 fgColor, u8 bgColor, bool blink) {
    vgacon_printSizedColorString(str, strlen(str), fgColor, bgColor, blink);
}

#define VPRINTF(fmt, args) do { va_start (args, fmt); vprintf (fmt, args); va_end (args); } while (0)

void vgacon_print(const char *fmt, ...) {
    va_list args;
    printf("      \xB3");
    VPRINTF(fmt, args);
}

void vgacon_printOK(const char *fmt, ...) {
    va_list args;
    putch(' '); vgacon_printColorString("   OK", VGACON_COLOR_GREEN, VGACON_COLOR_BLACK, false); putch('\xB3');
    VPRINTF(fmt, args);
}

void vgacon_printWarning(const char *fmt, ...) {
    va_list args;
    putch(' '); vgacon_printColorString(" WARN", VGACON_COLOR_YELLO, VGACON_COLOR_BLACK, false); putch('\xB3');
    VPRINTF(fmt, args);
}

void vgacon_printError(const char *fmt, ...) {
    va_list args;
    putch(' '); vgacon_printColorString("ERROR", VGACON_COLOR_RED, VGACON_COLOR_BLACK, false); putch('\xB3');
    VPRINTF(fmt, args);
}

void vgacon_printDebug(const char *fmt, ...) {
    va_list args;
    putch(' '); vgacon_printColorString("DEBUG", VGACON_COLOR_BLUE, VGACON_COLOR_BLACK, false); putch('\xB3');
    VPRINTF(fmt, args);
}

void vgacon_fillColorCharacter(char character, size_t length, u8 fgColor, u8 bgColor, bool blink) {
    vgacon_BIOSChar current;
    vgacon_BIOSChar _far* drawPtr = vgacon_getCurrentDrawPointer();

    current.c = character;
    current.attr = VGACON_MAKE_COLOR(fgColor, bgColor, blink);

    vgacon_advanceCursor(length);

    while (length--) {
        *drawPtr++ = current;
    }
}

void vgacon_fillCharacter(char character, size_t length) {
    vgacon_BIOSChar _far* drawPtr = vgacon_getCurrentDrawPointer();

    vgacon_advanceCursor(length);

    while (length--) {
        drawPtr++->c = character;
    }
}

bool vgacon_isCursorAtStartOfLine(void) {
    return VGACON_CURRENT_CURSOR_POSITION.x == 0;
}

u16 vgacon_getConsoleWidth(void) {
    return *vgacon_MEM_ColumnsOnScreen;
}

u16 vgacon_getConsoleHeight(void) {
    return (u16) (*vgacon_MEM_HighestColumnIndex + 1);
}

void vgacon_waitKeyWithMessage(void) {
    printf("< press any key to continue... >\n");
    getch();
}
