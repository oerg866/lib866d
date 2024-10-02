/*  LIB866D
    Utility Functions

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stddef.h>
#include "types.h"

#define MK_FP(seg,off) ((void _far *) (((u32) (seg) << 16) | ((u16) (off))))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define UNUSED_ARG(_arg_) (void) _arg_

typedef struct {
    const char *logoData;
    size_t width;
    size_t height;
    u8 fgColor;
    u8 bgColor;
} util_ApplicationLogo;

bool util_stringEquals(const char *str1, const char *str2);
bool util_stringStartsWith(const char *full, const char *toCheck);
void util_stringReplaceChar(char *str, char oldChar, char newChar);

/* strncasecmp implementation for C89 */
int util_strncasecmp(const char *str1, const char *str2, size_t strLen);

/*  Prints text and wraps around an ASCII logo on the left of the screen. This can only be done once per session.
    After the logo has been fully printed, this becomes a simple printf.
    Note: There will always be a blank character at the start of the line due to a DOS printing quirk workaround. */
void util_printWithApplicationLogo(const util_ApplicationLogo *logo, const char *fmt, ...);

#endif
