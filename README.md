# Disclaimer

This library is ***very*** incomplete and under active development.

Don't blame me if things explode from using this library, etc. (I hate legal talk...)

# lib866d

**lib866d** is a library for writing real-mode DOS software. It provides many functions for writing low level system software for DOS and compatible operating systems, and is intended primarily for utility and driver development.

## Features

* `386ASM.H`: Macros to use 32-Bit register and other 386+ level opcodes in inline-assembly
* `ARGS`: Sophisticated Program Argument Parsing
* `CPU_K6`: Low-level helper tools for manipulating **AMD K6 Family CPU features**
    * EPMR, multiplier, MTRR, Write Order/Allocate, L1/L2 Cache
* `DEBUG.H`: Assertions and debugging features
* `PCI`: PCI Device access
* `SYS`: Low-level system configuration and hardware detection functions
    * System memory detection
    * 32-Bit Port I/O
    * CPUID reading, CPU MSR R/W, CPU Control Register R/W
* `UTIL`: Generic utility functions (string manipulation, etc)
* `VESABIOS`: Functions for getting VESA BIOS data and mode information
* `VGACON`: Functions for interfacing with the VGA text console and colorful string printing

## Data types

The `TYPES.H` header provides generic access to all the data types.

To make things neat and concise, the data types are named intuitively:

* `u8, u16, u32`: Unsigned integer types
* `i8, i16, i32`: Signed integer types
* `bool`: Implementation for a boolean data type; implemented as an `enum` with two entries.

## Supported compilers

- [x] Microsoft C / C++ Version 6 / 7
- [x] Borland Turbo C 3.xx
- [x] OpenWatcom 2

## How to use

There are only a few basic steps necessary to integrate **lib866d** into your project:

* Add `lib866d` to your include path
* Use the library in your code
* Compile the C files (e.g. using a wildcard `LIB866D\\*.C`) and link the appropriate objects with your program

# License

[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc/4.0/deed.en)
