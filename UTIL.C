/*  LIB866D
    Utility Functions

    (C) 2024 E. Voirin (oerg866)
*/

#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "util.h"
#include "vgacon.h"

#define __LIB866D_TAG__ "UTIL.C"
#include "debug.h"

bool util_stringEquals(const char *str1, const char *str2) {
    return (bool) strcmp(str1, str2) == 0;
}

bool util_stringStartsWith(const char *full, const char *toCheck) {
    return (bool) strncmp(toCheck, full, strlen(toCheck)) == 0;
}

void util_stringReplaceChar(char *str, char oldChar, char newChar) {
    while (*str != 0x00) {
        *str = (char) ((*str == oldChar) ? newChar : *str);
        str++;
    }
}

void util_swapInPlace16(u16 *buf) {
    *buf = SWAP16(*buf);
}

void util_swapInPlace32(u32 *buf) {
    *buf = SWAP32(*buf);
}

int util_strncasecmp(const char *str1, const char *str2, size_t strLen) {
    while (strLen--) {
        int c1 = (int) tolower(*str1);
        int c2 = (int) tolower(*str2);

        if (c1 != c2 || c1 == '\0' || c2 == '\0') {
            return c1 - c2;
        }

        str1++;
        str2++;
    }
    return 0;
}

int util_snprintf(char *out, size_t size, const char *fmt, ...) {
    int toWrite = 0;
    char *outTmp = NULL;
    va_list args;

    va_start(args, fmt);
    toWrite = vsprintf(NULL, fmt, args);

    if (toWrite <= 0) {
        out[0] = 0x00;
        return toWrite;
    }

    outTmp = (char *) malloc(toWrite);
    L866_NULLCHECK(outTmp);

    vsprintf(outTmp, fmt, args);
    strncpy(out, outTmp, size);
    va_end(args);
    free(outTmp);

    if ((size_t) toWrite < size) {
        return toWrite;
    }
    return (int) size;
}

void util_printWithApplicationLogo(const util_ApplicationLogo *logo, const char *fmt, ...) {
    static size_t logoLinesShown = 0;
    const char *logoLinePtr;
    va_list args;

    L866_NULLCHECK(logo);
    L866_NULLCHECK(logo->logoData);

    logoLinePtr = &logo->logoData[logoLinesShown * logo->width];

    if (vgacon_isCursorAtStartOfLine() && logoLinesShown < logo->height) {
        putchar(' '); /* work around scrolling color attribute bug, we always leave a space ... */
        vgacon_printSizedColorString(logoLinePtr, logo->width, logo->fgColor, logo->bgColor, false);
        logoLinesShown++;
    }

    va_start (args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

i32 util_round(float f) {
    if (f > 0.0f)
        return (i32)(f + 0.5f);
    else
        return (i32)(f - 0.5f);
}

void util_sleep(u32 milliseconds) {
    clock_t start_time = clock();
    clock_t end_time = util_getTimeOffsetInClocks(milliseconds);
    
    while (clock() < (end_time)){};
}

u32 util_msToClocks(u32 milliseconds) {
    return (milliseconds * ((u32) (CLOCKS_PER_SEC))) / 1000UL;
}

u32 util_getTimeOffsetInClocks(u32 milliseconds) {
    return clock() + util_msToClocks(milliseconds);
}
