/* Generic DEBUG print helper */

#ifdef DEBUG
# include "vgacon.h"
# define DBG vgacon_printDebug
#else
# define DBG (void)
#endif

#ifndef _866ASSERT_H_
#define _866ASSRT_H_

#include <stdio.h>
#include <stdlib.h>

# if 1 /* #ifdef PRETTY_ASSERT */

#include "vgacon.h"

#pragma warning(disable: 4505) /* MS C: unreferenced local function */

#ifndef __LIB866D_TAG__
#error Please define __LIB866D_TAG__ for PRETTY_ASSERT support.
#endif
static const char _LIB866D_TAG[] = __LIB866D_TAG__;

static void __866_assert(const char *assertion, const char *error) {
    static const char errHdr[] = "\xF0\xF0\xF0\xF0\xF0\xF0\xF0\xF0\xF0 LIB866D FATAL ERROR \xF0\xF0\xF0\xF0\xF0\xF0\xF0\xF0";
    static const char errFmt[] = "\n"
                                 " \x10 Module:   [ %s ]\n"
                                 " \x10 Assertion [ %s ] failed!\n"
                                 "%s%s%s"
                                 " \x10 Aborting...\n ";
    printf("\n ");
    vgacon_printColorString(errHdr, VGACON_COLOR_RED, VGACON_COLOR_BLACK, true);
    printf(errFmt, _LIB866D_TAG, assertion, error ? " \x10 ": "", error ? error : "", error ? "\n" : "");
    vgacon_fillColorCharacter((char)0xF0, 38, VGACON_COLOR_RED, VGACON_COLOR_BLACK, true);
    abort();
}

#define L866_ASSERTM(x, error) ((void)((x) || (__866_assert(#x, error),0)))
#define L866_ASSERT(x)         ((void)((x) || (__866_assert(#x, NULL),0)))
#define L866_NULLCHECK(x)      L866_ASSERTM(x != NULL, "NULL POINTER")
#endif

#endif
