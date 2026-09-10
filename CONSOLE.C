/*  LIB866D
    Text Console Output

    (C) 2026 E. Voirin (oerg866)
*/

#include <dos.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "types.h"

#define CON_LINE_MAX    1024

/*  Every message sits in a column to the right of a status gutter: a five
    character tag, right aligned, followed by a vertical bar. Lines with
    nothing to report get a blank tag, so the text stays lined up. */
#define CON_TAG_BLANK   "     "
#define CON_TAG_DEBUG   "DEBUG"
#define CON_TAG_OK      "   OK"
#define CON_TAG_WARNING " WARN"
#define CON_TAG_ERROR   "ERROR"
#define CON_GUTTER      "\xB3"  /* CP437 Single Vertical Line */

/*  Width of the gutter itself: the tag plus the bar after it. */
#define CON_GUTTER_WIDTH 6

/*  BIOS Data Area */
#define BDA_COLUMNS     MK_FP(0x0040, 0x004A)   /* u16: text columns          */
#define BDA_LAST_ROW    MK_FP(0x0040, 0x0084)   /* u8:  rows - 1 (EGA and up) */
#define BDA_PAGE        MK_FP(0x0040, 0x0062)   /* u8:  active display page   */

/*  Console level (default = INFO) */
static con_Level _s_level = con_levelInfo;

/*  IMPORTANT: The attribute byte DOS was using when we started.
    This is sampled at the start from the character under the cursor.
    
    Everything we print that ISN'T DELIBERATELY COLORED uses this. */
static bool _s_defaultRead      = false;
static u8   _s_defaultAttribute = 0x07;

/*  Indicates whether the cursor is sitting at the left edge (and thus the 
    gutter should be drawn). */
static bool _s_atLineStart      = true;

static void con_restore(void);

void con_setLevel(con_Level level) {
    _s_level = level;
}

con_Level con_getLevel(void) {
    return _s_level;
}

u16 con_getColumns(void) {
    u16 _far *columns = (u16 _far *) BDA_COLUMNS;
    /*  If this is 0 somehow, BDA isn't set up as we thought, so return a sane default of 80. */
    return (*columns == 0) ? 80 : *columns;
}

u8 con_getRows(void) {
    u8 _far *lastRow = (u8 _far *) BDA_LAST_ROW;
    /* Ditto except this can also happen on pre-EGA hardware (wow) */
    return (*lastRow == 0) ? 25 : (u8) (*lastRow + 1);
}

static u8 con_getPage(void) {
    u8 _far *page = (u8 _far *) BDA_PAGE;
    return *page;
}

/*  Read the attribute of the character cell under the cursor. */
static u8 con_getAttributeAtCursor(void) {
    union REGS regs;

    regs.h.ah = 0x08;
    regs.h.bh = con_getPage();
    int86(0x10, &regs, &regs);

    /* AH = attribute, AL = character */
    return regs.h.ah;
}

static void con_setCursor(u8 row, u8 column) {
    union REGS regs;

    regs.h.ah = 0x02;
    regs.h.bh = con_getPage();
    regs.h.dh = row;
    regs.h.dl = column;
    int86(0x10, &regs, &regs);
}

con_Position con_getCursor(void) {
    con_Position ret = { 0, 0 };
    union REGS regs;

    regs.h.ah = 0x03;
    regs.h.bh = con_getPage();
    int86(0x10, &regs, &regs);

    ret.y = regs.h.dh;
    ret.x = regs.h.dl;

    return ret;
}

/*  Scroll the whole screen up by one line, filling the next line with spaces
    in the given attribute. */
static void con_scrollUp(u8 attribute) {
    union REGS regs;

    regs.h.ah = 0x06;
    regs.h.al = 0x01;       /* lines to scroll        */
    regs.h.bh = attribute;  /* attribute for new line */
    regs.h.ch = 0;          /* top left row           */
    regs.h.cl = 0;          /* top left column        */
    regs.h.dh = (u8) (con_getRows() - 1);
    regs.h.dl = (u8) (con_getColumns() - 1);
    int86(0x10, &regs, &regs);
}

/*  Write a character at the cursor with the given attribute.
    Cursor is not moved, no scrolling / wrapping. */
static void con_putCharAt(char c, u8 attribute) {
    union REGS regs;

    regs.h.ah = 0x09;
    regs.h.al = (u8) c;
    regs.h.bh = con_getPage();
    regs.h.bl = attribute;
    regs.x.cx = 1;          /* repeat count */
    int86(0x10, &regs, &regs);
}

/*  Screen output only makes sense when we are outputting to the screen.
    This function checks whether we're redirected. */
static bool con_isRedirected(void) {
    return isatty(fileno(stdout)) == 0;
}

bool con_ownsScreen(void) {
    return !con_isRedirected();
}

/*  Fill one line with spaces in the given attribute. */
static void con_clearLine(u8 row, u8 attribute) {
    union REGS regs;

    regs.h.ah = 0x06;
    regs.h.al = 0x00;           /* zero lines to scroll means clear the window */
    regs.h.bh = attribute;
    regs.h.ch = row;
    regs.h.cl = 0;
    regs.h.dh = row;
    regs.h.dl = (u8) (con_getColumns() - 1);
    int86(0x10, &regs, &regs);
}

/*  Sample the current character attribute (at the first invocation of public con_ funcs).
    An atexit handler is set up to restore this after program ends. */
static u8 con_getDefaultAttribute(void) {
    u8 attribute;

    if (!_s_defaultRead) {
        attribute = con_getAttributeAtCursor();

        /*  An invisible foreground means the cell we sampled was never
            written by the shell, so fall back to the usual DOS default
            rather than printing black on black. */
        _s_defaultAttribute = ((attribute & 0x0F) == ((attribute >> 4) & 0x0F))
                            ? 0x07 : attribute;
        _s_defaultRead      = true;

        atexit(con_restore);
    }

    return _s_defaultAttribute;
}

/*  Get the attribute byte for a foreground/background color,
    CON_COLOR_DEFAULT will yield the default from when the program was started - 
    for that "layer" anyway */
static _inline u8 con_colorsToAttributeByte(u8 foreground, u8 background) {
    u8 fallback = con_getDefaultAttribute();
    return (u8) ((((background == CON_COLOR_DEFAULT) ? (u8) ((fallback >> 4) & 0x0F) : (u8) (background & 0x0F)) << 4)
                | ((foreground == CON_COLOR_DEFAULT) ? (u8) (fallback & 0x0F) : (u8) (foreground & 0x0F)));
}

void con_colorText(const char *text, u8 foreground, u8 background) {
    con_colorTextSized(text, strlen(text), foreground, background);
}

void con_colorTextSized(const char *text, size_t length, u8 foreground, u8 background) {
    /*  Either half of the attribute may be left to the shell's own choice. */
    u8              attribute   = con_colorsToAttributeByte(foreground, background);
    u16             columns     = con_getColumns();
    u8              rows        = con_getRows();
    con_Position    cursor      = con_getCursor();
    u8              row         = cursor.y;
    u8              column      = cursor.x;

    if (text == NULL) return;

    if (con_isRedirected()) {
        if (length > 0) _s_atLineStart = (text[length - 1] == '\n');

        while (length-- && *text != '\0') {
            fputc(*text, stdout);
            text++;
        }

        return;
    }

    for (; length > 0 && *text != '\0'; text++) {
        length--;

        if (*text == '\r') {
            column = 0;
            continue;
        }

        if (*text != '\n') {
            con_setCursor(row, column);
            con_putCharAt(*text, attribute);
            column++;

            if (column < columns) continue;
        }

        /* End of line, either explicit or because we ran off the right edge. */
        column = 0;
        row++;

        if (row >= rows) {
            con_scrollUp(attribute);
            row = (u8) (rows - 1);
        }
    }

    con_setCursor(row, column);
    _s_atLineStart = (column == 0);
}

void con_colorFill(char toFill, size_t length, u8 foreground, u8 background) {
    /*  Either half of the attribute may be left to the shell's own choice. */
    u8              attribute   = con_colorsToAttributeByte(foreground, background);
    u16             columns     = con_getColumns();
    u8              rows        = con_getRows();
    con_Position    cursor      = con_getCursor();
    u8              row         = cursor.y;
    u8              column      = cursor.x;

    if (con_isRedirected()) {
        if (length > 0) _s_atLineStart = (toFill == '\n');

        while (length--) {
            fputc(toFill, stdout);
        }

        return;
    }

    while (length--) {

        con_setCursor(row, column);
        con_putCharAt(toFill, attribute);
        column++;

        if (column < columns) continue;

        /* End of line, either explicit or because we ran off the right edge. */
        column = 0;
        row++;

        if (row >= rows) {
            con_scrollUp(attribute);
            row = (u8) (rows - 1);
        }
    }

    con_setCursor(row, column);
    _s_atLineStart = (column == 0);
}

/*  Hand the screen back the way we found it. We move to a fresh line and give
    every cell on it the shell's original attribute, because the BIOS teletype
    the shell prints through takes its color from the cell it overwrites: a
    line still carrying our colors would tint the next command's output. */
static void con_restore(void) {
    u8              rows = con_getRows();
    con_Position    cursor = con_getCursor();
    u8              row = cursor.y;
    u8              column = cursor.x;

    if (!_s_defaultRead || con_isRedirected()) return;

    if (column != 0) row++;

    if (row >= rows) {
        con_scrollUp(_s_defaultAttribute);
        row = (u8) (rows - 1);
    }

    con_clearLine(row, _s_defaultAttribute);
    con_setCursor(row, 0);

    _s_atLineStart = true;
}

/*  Format into a static line buffer. There is only ever one message being
    built at a time, and a 256 byte automatic buffer is a lot to ask of a
    real mode stack. */
static char *con_format(const char *fmt, va_list args) {
    static char line[CON_LINE_MAX];

    vsprintf(line, fmt, args);
    line[CON_LINE_MAX - 1] = '\0';
    return line;
}

/*  Draw the status gutter at the left edge. A NULL tag leaves the status
    field blank, which is what continuation lines and plain output want. */
static void con_emitGutter(const char *tag, u8 tagColor) {
    if (tag == NULL) {
        con_colorText(CON_TAG_BLANK, CON_COLOR_DEFAULT, CON_COLOR_DEFAULT);
    } else {
        con_colorText(tag, tagColor, CON_COLOR_DEFAULT);
    }

    con_colorText(CON_GUTTER, CON_COLOR_DEFAULT, CON_COLOR_DEFAULT);
}


/*  Emit a message into the column right of the gutter, drawing the gutter
    for every line the message starts. The tag is only spent on the first of
    them: a multi line message is one event, not several.

    The body goes through con_colorText rather than through fputs. The BIOS
    teletype call that stdout ends up in does not take an attribute in text
    mode: it re-uses whatever attribute already sits in the character cell it
    is about to overwrite. Plain text would therefore come out in the color of
    whatever the shell happened to leave on that part of the screen. Writing
    it ourselves in the shell's own attribute keeps it consistent. */
static void con_emit(con_Level level, const char *tag, u8 tagColor, const char *fmt, va_list args) {
    char *segment;
    char *newline;

    if (level < _s_level) return;

    for (segment = con_format(fmt, args); *segment != '\0'; ) {
        newline = strchr(segment, '\n');
        if (newline != NULL) *newline = '\0';

        /*  A blank line still gets its gutter, so that the rule down the left
            of the screen runs unbroken through the gaps between messages
            rather than being chopped into pieces by them. */
        if (_s_atLineStart) {
            con_emitGutter(tag, tagColor);
            _s_atLineStart = false;
            tag            = NULL;
        }

        if (*segment != '\0') {
            con_colorText(segment, CON_COLOR_DEFAULT, CON_COLOR_DEFAULT);
        }

        if (newline == NULL) break;

        con_colorText("\n", CON_COLOR_DEFAULT, CON_COLOR_DEFAULT);
        _s_atLineStart = true;
        segment        = newline + 1;
    }
}

void con_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    con_emit(con_levelDebug, CON_TAG_DEBUG, CON_COLOR_DGRAY, fmt, args);
    va_end(args);
}

void con_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    con_emit(con_levelInfo, NULL, 0, fmt, args);
    va_end(args);
}

void con_plain(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    con_colorText(con_format(fmt, args), CON_COLOR_DEFAULT, CON_COLOR_DEFAULT);
    va_end(args);
}

void con_ok(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    con_emit(con_levelOk, CON_TAG_OK, CON_COLOR_LGREEN, fmt, args);
    va_end(args);
}

void con_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    con_emit(con_levelWarning, CON_TAG_WARNING, CON_COLOR_YELLOW, fmt, args);
    va_end(args);
}

void con_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    con_emit(con_levelError, CON_TAG_ERROR, CON_COLOR_LRED, fmt, args);
    va_end(args);
}

void con_putc(char c) {
    con_colorTextSized(&c, 1, CON_COLOR_DEFAULT, CON_COLOR_DEFAULT);
}