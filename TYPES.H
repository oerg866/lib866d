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

typedef enum { false, true } bool;

#define U8_MAX UCHAR_MAX
#define I8_MAX SCHAR_MAX
#define I8_MIN SCHAR_MIN
#define U16_MAX USHRT_MAX
#define I16_MAX SHRT_MAX
#define I16_MIN SHRT_MIN
#define U32_MAX ULONG_MAX
#define I32_MIN LONG_MIN
#define I32_MAX LONG_MAX

#endif
