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

static inline bool util_dynU16Grow(DynU16 *arr) {
    arr->capacity = (arr->capacity == 0) ? 8 : (arr->capacity * 2);
    arr->items = realloc(arr->items, arr->capacity);
    return arr->items != NULL;
}

bool util_dynU16Add(DynU16 *arr, u16 val) {
    L866_NULLCHECK(arr);

    if ((arr->count == arr->capacity) && !util_dynU16Grow(arr)) return false;
    arr->items[arr->count++] = val;
    return true;;
}

bool util_dynU16Contains(const DynU16 *arr, u16 val, size_t *index) {
    size_t i;
    L866_NULLCHECK(arr);
    for (i = 0 ; i < arr->count; i++) {
        if (arr->items[i] == val) {
            if (index != NULL) *index = i;
            return true;
        }
    }
    return false;
}

void util_dynU16Remove(DynU16 *arr, size_t index) {
    L866_NULLCHECK(arr);
    L866_ASSERT(index < arr->count);
    memmove(&arr->items[index], &arr->items[index + 1], (arr->count - (index + 1)) * sizeof(u16));
    arr->count--;
}

static int compareU16(const void *a, const void *b) {
    u16 arg1 = *(const u16 *)a;
    u16 arg2 = *(const u16 *)b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}


void util_dynU16Sort(DynU16 *arr) {
    L866_NULLCHECK(arr);
    if (arr->count <= 1) {
        return; // No need to sort if there are 0 or 1 elements
    }
    qsort(arr->items, arr->count, sizeof(u16), compareU16);
}

void util_dynU16RemoveDuplicates(DynU16 *arr) {
    size_t i, j;
    L866_NULLCHECK(arr);

    for (i = 0; i < arr->count; i++) {
        for (j = i + 1; j < arr->count; ) {
            if (arr->items[j] == arr->items[i]) {
                util_dynU16Remove(arr, j);
            } else {
                j++;
            }
        }
    }
}

void util_dynU16Free(DynU16 *arr) {
    L866_NULLCHECK(arr);
    if (arr->items != NULL) {
        free(arr->items);
        arr->items = NULL;
        arr->count = 0;
        arr->capacity = 0;
    }
}
