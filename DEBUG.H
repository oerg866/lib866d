/*  LIB866D
    Debugging helpers and assertions

    (C) 2024 E. Voirin (oerg866)
*/

/* Generic DEBUG print helper */

#ifndef ASSERT_ENABLE
#define ASSERT_ENABLE
#endif

#ifdef DEBUG
# include "console.h"
# define DBG con_debug
# ifndef ASSERT_ENABLE
#  define ASSERT_ENABLE
# endif
#else
#define __BREAK
static _inline void DBG(const char *fmt, ...) { (void)fmt; }
#endif

#ifndef _866ASSERT_H_
#define _866ASSERT_H_

#ifdef _MSC_VER
#pragma warning(disable: 4505) /* MS C: unreferenced local function */
#endif

#ifdef ASSERT_ENABLE

#include <stdio.h>
#include <stdlib.h>

#include "console.h"

#ifndef __LIB866D_TAG__
#error Please define __LIB866D_TAG__ for PRETTY_ASSERT support.
#endif

//static const char _LIB866D_TAG[] = __LIB866D_TAG__;
static void __866_assert(const char *assertion, const char *error) {
    static const char errHdr[] = "\xF0 LIB866D FATAL ERROR \xF0";
    static const char errFmt[] = "\n"
                                 " \x10 Module:   [ %s ]\n"
                                 " \x10 Assertion [ %s ] failed!\n"
                                 "%s%s%s"
                                 " \x10 Aborting...\n ";
    printf("\n ");
    con_colorText(errHdr, CON_COLOR_RED, CON_COLOR_BLACK);
    printf(errFmt, __LIB866D_TAG__, assertion, error ? " \x10 ": "", error ? error : "", error ? "\n" : "");
    con_colorFill(0xF0, 38, CON_COLOR_RED, CON_COLOR_DGRAY);
    printf("\n ");
    abort();
}

#define NOTNULL(x)             ((x) != (NULL))
#define L866_ASSERTM(x, error) ((void)((x) || (__866_assert(#x, error),0)))
#define L866_ASSERT(x)         ((void)((x) || (__866_assert(#x, NULL),0)))
#define L866_NULLCHECK(x)      L866_ASSERTM(NOTNULL(x), "Null ptr")

#else
#define L866_ASSERTM(x, error)
#define L866_ASSERT(x)
#define L866_NULLCHECK(x)
#endif

#endif
