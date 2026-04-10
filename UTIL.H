/*  LIB866D
    Utility Functions

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stddef.h>
#include <dos.h>
#include "types.h"

#ifndef MK_FP
#define MK_FP(seg,off) ((void _far *) (((u32) (seg) << 16) | ((u16) (off))))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define UNUSED_ARG(_arg_) (void) _arg_

#define SWAP16(x)   ((u16)(((x) >> 8) & 0x00FF) | \
                          (((x) << 8) & 0xFF00))

#define SWAP32(x)   ((u32)(((x) >> 24) & 0x000000FFUL) | \
                          (((x) >>  8) & 0x0000FF00UL) | \
                          (((x) <<  8) & 0x00FF0000UL) | \
                          (((x) << 24) & 0xFF000000UL))

#define BIT(x)      (1 << (x))
#define BIT8(x)     ((u8)BIT(x))
#define BIT32(x)    (1UL << (x))
/* Set bit<idx> in <x> if <val> evaluates to TRUE */
#define BIT_SET(x, idx, val)  ((val) ? (x | BIT(idx)) : (x & ~BIT(idx)))

typedef struct {
    const char *logoData;
    size_t width;
    size_t height;
    u8 fgColor;
    u8 bgColor;
} util_ApplicationLogo;

#define DYNARRAY_DEF(_NAME, _TYPE) typedef struct _NAME { size_t count, capacity; _TYPE ## * items; } _NAME

DYNARRAY_DEF(DynU8,     u32);       /* Dynamic array of Unsigned  8-Bit integers */
DYNARRAY_DEF(DynU16,    u16);       /* Dynamic array of Unsigned 16-Bit integers */
DYNARRAY_DEF(DynU32,    u16);       /* Dynamic array of Unsigned 32-Bit integers */
DYNARRAY_DEF(DynStr,    char *);    /* Dynamic array of strings (so an array of pointers) */

/*  Returns true if str1 and str2 are identical in content */
bool util_stringEquals(const char *str1, const char *str2);
/*  Returns true if <full> starts with the complete content of <toCheck> */
bool util_stringStartsWith(const char *full, const char *toCheck);
/*  Returns true if <full> ends with the complete content of <toCheck> */
bool util_stringEndsWith(const char *full, const char *toCheck);
/*  Replaces all instances of <oldChar> with <newChar> in <str> */
void util_stringReplaceChar(char *str, char oldChar, char newChar);
/*  Attempts to parse <str> as an unsigned 32 bit number.
    The conventions of strto<X> with base 0 applies, so 0x-hexadecimals are handled, for example. */
bool util_stringToU32(const char *str, u32 *out);

/*  Performs a 16-bit byteswap in place at <buf> */
void util_swapInPlace16(u16 *buf);
/*  Performs a 32-bit byteswap in place at <buf> */
void util_swapInPlace32(u32 *buf);

/*  strncasecmp implementation for C89 */
int util_strncasecmp(const char *str1, const char *str2, size_t strLen);
/*  snprintf implementation for C89 (kludgey and slow, be warned) */
int util_snprintf(char *out, size_t size, const char *fmt, ...);

/*  Prints text and wraps around an ASCII logo on the left of the screen. This can only be done once per session.
    After the logo has been fully printed, this becomes a simple printf.
    Note: There will always be a blank character at the start of the line due to a DOS printing quirk workaround. */
void util_printWithApplicationLogo(const util_ApplicationLogo *logo, const char *fmt, ...);

/* Rounds to the nearest integer */
i32 util_round(float f);

/* Sleep for n milliseconds (may be inaccurate) */
void util_sleep(u32 milliseconds);
/* Converts milliseconds to CLOCKS */
u32 util_msToClocks(u32 milliseconds);
/* Gets the clock() count value that corresponds to current time + <milliseconds> */
u32 util_getTimeOffsetInClocks(u32 milliseconds);

/* Dynamic array functions */
bool util_dynU8Add          (DynU8  *arr, u8 val);              /*  Add value to a dynamic array */
void util_dynU8Free         (DynU8  *arr);                      /*  Release the memory allocated by the dynamic array */
void util_dynU8Remove       (DynU8  *arr, size_t index);        /*  Remove value from the dynamic array */
void util_dynU8Sort         (DynU8  *arr);                      /*  Sort the dynamic array */
bool util_dynU8Contains     (DynU8  *arr, u8 val, size_t *at);  /*  Check if dynamic array contains value */
void util_dynU8Deduplicate  (DynU8  *arr);                      /*  Remove all duplicate entries from array so all entries are unique */

bool util_dynU16Add         (DynU16 *arr, u16 val);             /*  Add value to a dynamic array */
void util_dynU16Free        (DynU16 *arr);                      /*  Release the memory allocated by the dynamic array */
void util_dynU16Remove      (DynU16 *arr, size_t index);        /*  Remove value from the dynamic array */
void util_dynU16Sort        (DynU16 *arr);                      /*  Sort the dynamic array */
bool util_dynU16Contains    (DynU16 *arr, u16 val, size_t *at); /*  Check if dynamic array contains value */
void util_dynU16Deduplicate (DynU16 *arr);                      /*  Remove all duplicate entries from array so all entries are unique */

bool util_dynU32Add         (DynU32 *arr, u32 val);             /*  Add value to a dynamic array */
void util_dynU32Free        (DynU32 *arr);                      /*  Release the memory allocated by the dynamic array */
void util_dynU32Remove      (DynU32 *arr, size_t index);        /*  Remove value from the dynamic array */
void util_dynU32Sort        (DynU32 *arr);                      /*  Sort the dynamic array */
bool util_dynU32Contains    (DynU32 *arr, u32 val, size_t *at); /*  Check if dynamic array contains value */
void util_dynU32Deduplicate (DynU32 *arr);                      /*  Remove all duplicate entries from array so all entries are unique */

bool util_dynStrAdd         (DynStr *arr, const char *str);     /*  Add string to a dynamic string array (duplicates) */
void util_dynStrFree        (DynStr *arr);                      /*  Release the memory allocated by the dynamic array & all strings */
void util_dynStrRemove      (DynStr *arr, size_t index);        /*  Remove string from the dynamic array & free it */
//TODO void util_dynStrSort        (DynStr *arr);               /*  Sort the dynamic array */
bool util_dynStrContains    (DynStr *arr, const char *str);     /*  Check if dynamic array contains value */
//TODO void util_dynStrDeduplicate (DynStr *arr);               /*  Remove all duplicate entries from array so all entries are unique */
void util_dynStrReset       (DynStr *arr);                      /* Prepares dynamic array for re-use without re-allocating, keeping the capacity. */

/*  Special variant of dynStrAdd that acts like a printf. The resulting string will be capped at 255 characters */
bool util_dynStrAddF        (DynStr *arr, const char *fmt, ...);

#endif
