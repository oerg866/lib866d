/*  LIB866D
    Debugging helpers and assertions

    (C) 2024 E. Voirin (oerg866)
*/

/* Generic DEBUG print helper */

#ifdef DEBUG
# include "vgacon.h"
# define DBG vgacon_printDebug
# ifndef ASSERT_ENABLE
#  define ASSERT_ENABLE
# endif
#else
static void DBG(const char *fmt, ...) { (void)fmt; }
#endif

#ifndef _866ASSERT_H_
#define _866ASSERT_H_


#pragma warning(disable: 4505) /* MS C: unreferenced local function */

#ifdef ASSERT_ENABLE

#include <stdio.h>
#include <stdlib.h>

#include "vgacon.h"

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
    printf("\n ");
    abort();
}

#define L866_ASSERTM(x, error) ((void)((x) || (__866_assert(#x, error),0)))
#define L866_ASSERT(x)         ((void)((x) || (__866_assert(#x, NULL),0)))
#define L866_NULLCHECK(x)      L866_ASSERTM((void _far *) (x) != (void _far *) (NULL), "NULL POINTER")

#else
#define L866_ASSERTM(x, error)
#define L866_ASSERT(x)
#define L866_NULLCHECK(x)
#endif

#endif
