/*  LIB866D
    Sized Data Types Header for 16 Bit Applications

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef TYPES_H
#define TYPES_H

#include <limits.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;
typedef signed   char  i8;
typedef signed   short i16;
typedef signed   long  i32;
typedef signed   char  s8;  // compatibility
typedef signed   short s16; // compatibility
typedef signed   long  s32; // compatibility

#if defined(_MSC_VER) && (_MSC_VER > 700) && !defined(SUPER_STRICT)
/* This annoying warning pops up all the time */
#pragma warning(disable: 4761) /* MS C: integral size mismatch in argument */
#endif

#if defined(__BORLANDC__) || defined(__TURBOC__) || defined(_MSC_VER)
#if !defined(bool)
typedef enum { false, true } _myBool;
#define bool _myBool
#endif
#else
#include <stdbool.h>
#endif

#define U8_MAX UCHAR_MAX
#define I8_MAX SCHAR_MAX
#define I8_MIN SCHAR_MIN
#define U16_MAX USHRT_MAX
#define I16_MAX SHRT_MAX
#define I16_MIN SHRT_MIN
#define U32_MAX ULONG_MAX
#define I32_MIN LONG_MIN
#define I32_MAX LONG_MAX

/* Shut up VSCode about a few keywords.... */
#ifdef __INTELLISENSE__
#define _asm asm
#define _inline inline
#define _far
#define far
#define _huge
#define huge
#define cdecl
#define _interrupt
#endif

#endif
