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
    L866_NULLCHECK(str1);
    L866_NULLCHECK(str2);
    return (bool) strcmp(str1, str2) == 0;
}

bool util_stringStartsWith(const char *full, const char *toCheck) {
    L866_NULLCHECK(full);
    L866_NULLCHECK(toCheck);
    return (bool) strncmp(toCheck, full, strlen(toCheck)) == 0;
}

bool util_stringEndsWith(const char *full, const char *toCheck) {
    const char *start;
    L866_NULLCHECK(full);
    L866_NULLCHECK(toCheck);

    start = full + strlen(full) - strlen(toCheck);
    if (start < full) return false;
    return util_stringEquals(start, toCheck);
}

void util_stringReplaceChar(char *str, char oldChar, char newChar) {
    L866_NULLCHECK(str);
    while (*str != 0x00) {
        *str = (char) ((*str == oldChar) ? newChar : *str);
        str++;
    }
}

bool util_stringToU32(const char *str, u32 *out) {
    u32 value;
    char *endPtr;

    L866_NULLCHECK(str);
    L866_NULLCHECK(out);

    value = strtoul(str, &endPtr, 0);

    /* If endPtr points to the beginning of the string, no conversion occurred. */
    if (endPtr == str) {
        return false;
    }

    /* Check if the entire string was consumed. */
    if (*endPtr != 0x00) {
        return false;
    }

    *out = value;
    return true;
}

void util_swapInPlace16(u16 *buf) {
    L866_NULLCHECK(buf);
    *buf = SWAP16(*buf);
}

void util_swapInPlace32(u32 *buf) {
    L866_NULLCHECK(buf);
    *buf = SWAP32(*buf);
}

int util_strncasecmp(const char *str1, const char *str2, size_t strLen) {
    L866_NULLCHECK(str1);
    L866_NULLCHECK(str2);
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

typedef struct { size_t count; size_t capacity; u8 *items; } DynArray;

static bool util_dynArrayGrowGeneric(void *arr, size_t elementSize) {
    DynArray *da = (DynArray *) arr;
    void *newAlloc;
    size_t newCapacity;
    L866_NULLCHECK(arr);
    if ((da->count + 1) <= da->capacity) return true;

    newCapacity = (da->capacity == 0) ? 8 : (da->capacity * 2);

    DBG("grow: %p->items(%p) wants cap. %u has %u, elemsize %u count %u \n", arr, da->items,
        newCapacity, da->capacity, elementSize, da->count);

    /* First try to expand the block */
    if (_expand(da->items, newCapacity * elementSize)) {
        DBG("grow: EXPANDED %p to %u bytes\n", da->items, newCapacity * elementSize);
        da->capacity = newCapacity;
        return true;
    }

    /* That failed, so we try to alloc and copy instead */
    newAlloc = calloc(newCapacity, elementSize);

    if (NULL != newAlloc) {
        DBG("grow: REALLOCATED %p -> %p to %u bytes\n", da->items, newAlloc, newCapacity * elementSize);
        memcpy(newAlloc, da->items, da->count * elementSize);
        free(da->items);
        da->items = newAlloc;
        da->capacity = newCapacity;
        return true;
    }

    DBG("grow: FAILED reallocing %p to %u bytes\n", da->items, newCapacity * elementSize);
    return false;
}

static _inline void *util_dynArrayItemAtIndex(void *arr, size_t index, size_t elementSize) {
    DynArray *da = (DynArray *) arr;
    return da->items + (index * elementSize);
}

static _inline bool util_dynArrayAddGeneric(void *arr, void *val, size_t elementSize) {
    DynArray *da = (DynArray *) arr;
    L866_NULLCHECK(arr);
    if (!util_dynArrayGrowGeneric(arr, elementSize)) {
        return false;
    }
    da->count++;
    memcpy(util_dynArrayItemAtIndex(da, da->count - 1, elementSize), val, elementSize);
    return true;
}

static _inline void util_dynArrayRemoveGeneric(void *arr, size_t index, size_t elementSize) {
    DynArray *da = (DynArray *) arr;
    u8 *dst = util_dynArrayItemAtIndex(da, index, elementSize);
    u8 *src = dst + elementSize;
    L866_NULLCHECK(arr);
    L866_ASSERT(index < da->count);
    memmove(dst, src, (da->count - (index + 1)) * elementSize);
    da->count--;
}

static _inline void util_dynArrayFreeGeneric(void *arr) {
    DynArray *da = (DynArray *) arr;
    L866_NULLCHECK(arr);
    if (da->items != NULL) free(da->items);
    da->items = NULL;
    da->count = 0;
    da->capacity = 0;
}

static _inline bool util_dynArrayContainsGeneric(void *arr, void *val, size_t elementSize, size_t *at) {
    DynArray *da = (DynArray *) arr;
    size_t i;
    L866_NULLCHECK(arr);

    for (i = 0; i < da->count; i++) {
        void *srcData = util_dynArrayItemAtIndex(arr, i, elementSize);
        if (memcmp(srcData, val, elementSize) == 0) {
            if (at != NULL) *at = i;
            return true; 
        }
    }
    return false;
}

void util_dynArrayDeduplicateGeneric(void *arr, size_t elementSize) {
    DynArray *da = (DynArray *) arr;
    size_t i, j;
    L866_NULLCHECK(arr);

    for (i = 0; i < da->count; i++) {
        void *elemI = util_dynArrayItemAtIndex(arr, i, elementSize);
        for (j = i + 1; j < da->count; ) {
            void *elemJ = util_dynArrayItemAtIndex(arr, j, elementSize);
            if (0 == memcmp(elemJ, elemI, elementSize)) {
                util_dynArrayRemoveGeneric(arr, j, elementSize);
            } else {
                j++;
            }
        }
    }
}

#define util_dynArrayGrowOne(arr)           util_dynArrayGrowGeneric        (arr, 1, sizeof(arr->items[0]))
#define util_dynArrayAdd(arr, val)          util_dynArrayAddGeneric         (arr, &val, sizeof(arr->items[0]))
#define util_dynArrayFree(arr)              util_dynArrayFreeGeneric        (arr)
#define util_dynArrayRemove(arr, index)     util_dynArrayRemoveGeneric      (arr, index, sizeof(arr->items[0]))
#define util_dynArrayContains(arr, val, at) util_dynArrayContainsGeneric    (arr, &val, sizeof(arr->items[0]), at);
#define util_dynArrayDeduplicate(arr)       util_dynArrayDeduplicateGeneric (arr, sizeof(arr->items[0]))

static int _compareU8(const void *a, const void *b) {
    if (*(const u8 *)a < *(const u8 *)b) return -1;
    if (*(const u8 *)a > *(const u8 *)b) return 1;
    return 0;
}

static int _compareU16(const void *a, const void *b) {
    if (*(const u16 *)a < *(const u16 *)b) return -1;
    if (*(const u16 *)a > *(const u16 *)b) return 1;
    return 0;
}

static int _compareU32(const void *a, const void *b) {
    if (*(const u32 *)a < *(const u32 *)b) return -1;
    if (*(const u32 *)a > *(const u32 *)b) return 1;
    return 0;
}

#define _CHECK_AND_QSORT(arr, compareFunc) { L866_NULLCHECK(arr); qsort(arr->items, arr->count, sizeof(arr->items[0]), compareFunc); }

bool util_dynU8Add          (DynU8  *arr, u8  val)              { return util_dynArrayAdd(arr, val); }
bool util_dynU16Add         (DynU16 *arr, u16 val)              { return util_dynArrayAdd(arr, val); }
bool util_dynU32Add         (DynU32 *arr, u32 val)              { return util_dynArrayAdd(arr, val); }

void util_dynU8Free         (DynU8  *arr)                       { util_dynArrayFree(arr); }
void util_dynU16Free        (DynU16 *arr)                       { util_dynArrayFree(arr); }
void util_dynU32Free        (DynU32 *arr)                       { util_dynArrayFree(arr); }

void util_dynU8Remove       (DynU8  *arr, size_t index)         { util_dynArrayRemove(arr, index); }
void util_dynU16Remove      (DynU16 *arr, size_t index)         { util_dynArrayRemove(arr, index); }
void util_dynU32Remove      (DynU32 *arr, size_t index)         { util_dynArrayRemove(arr, index); }

void util_dynU8Sort         (DynU8  *arr)                       { _CHECK_AND_QSORT(arr, _compareU8); }
void util_dynU16Sort        (DynU16 *arr)                       { _CHECK_AND_QSORT(arr, _compareU16); }
void util_dynU32Sort        (DynU32 *arr)                       { _CHECK_AND_QSORT(arr, _compareU32); }

bool util_dynU8Contains     (DynU8  *arr, u8 val, size_t *at)   { return util_dynArrayContains(arr, val, at); }
bool util_dynU16Contains    (DynU16 *arr, u16 val, size_t *at)  { return util_dynArrayContains(arr, val, at); }
bool util_dynU32Contains    (DynU32 *arr, u32 val, size_t *at)  { return util_dynArrayContains(arr, val, at); }

void util_dynU8Deduplicate  (DynU8  *arr)                       { util_dynArrayDeduplicate(arr); }
void util_dynU16Deduplicate (DynU16 *arr)                       { util_dynArrayDeduplicate(arr); }
void util_dynU32Deduplicate (DynU32 *arr)                       { util_dynArrayDeduplicate(arr); }

/* Strings are arrays of pointers, so they need special functions */

bool util_dynStrAdd(DynStr *arr, const char *str) {
    char *ourCopy;
    L866_NULLCHECK(str);
    ourCopy = strdup(str);
    if (ourCopy == NULL) return false;
    return util_dynArrayAdd(arr, ourCopy);
}

void util_dynStrFree(DynStr *arr) {
    L866_NULLCHECK(arr);
    util_dynStrReset(arr);
    util_dynArrayFree(arr);
}

void util_dynStrRemove(DynStr *arr, size_t index) {
    if (arr == NULL || arr->items == NULL) return;
    if (index >= arr->count) return;
    /* Compared to the regular remove, we must also free the value itself first */
    free(arr->items[arr->count - 1]);
    util_dynArrayRemove(arr, index);
}

void util_dynStrSort(DynStr *arr) {
    /* Not implemented yet*/
    UNUSED_ARG(arr);
}

bool util_dynStrContains(DynStr *arr, const char *str) {
    size_t i;
    L866_NULLCHECK(arr);
    L866_NULLCHECK(str);
    for (i = 0; i < arr->count; i++) {
        if (util_stringEquals(arr->items[i], str)) return true;
    }
    return false;
}

bool util_dynStrAddF(DynStr *arr, const char *fmt, ...) {
    static char buf[256];
    va_list vl; \
    L866_NULLCHECK(fmt);
    va_start(vl, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, vl);
    va_end(vl);
    buf[255] = 0;
    return util_dynStrAdd(arr, buf);
}

void util_dynStrReset(DynStr *arr) {
    DBG("dynStr reset %u items\n", arr->count);

    while (arr->count) {
        arr->count--; 
        free(arr->items[arr->count]);
        arr->items[arr->count] = NULL;
    }
}
