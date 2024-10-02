/*  LIB866D
    VESA BIOS Extensions support header

    Info courtesy of http://wiki.osdev.org/Getting_VBE_Mode_Info   

    (C) 2024 E. Voirin (oerg866)
 */

#ifndef _VESABIOS_H_
#define _VESABIOS_H_

#include "types.h"

#pragma warning(disable: 4001) /* Non-'int' bitfields in Microsoft C */

#pragma pack(1)

typedef struct {
    u16 _dummy_0_   : 7;
    u16 hasLFB      : 1;
    u16 _dummy_1_   : 8;
} vesa_ExtraAttribs;

typedef struct {
    vesa_ExtraAttribs   attributes;             /* deprecated except bit 7 (=mode supports a linear frame buffer) */
    u8                  windowA;                /* deprecated */
    u8                  windowB;                /* deprecated */
    u16                 granularity;            /* deprecated; used while calculating bank numbers */
    u16                 windowSize;
    u16                 segmentA;
    u16                 segmentB;
    void          _far *winFuncPtr;             /* deprecated; used to switch banks from protected mode without returning to real mode */
    u16                 pitch;                  /* number of bytes per horizontal line */
    u16                 width;                  /* width in pixels */
    u16                 height;                 /* height in pixels */
    u8                  wChar;                  /* unused... */
    u8                  yChar;                  /* ... */
    u8                  planes;
    u8                  bpp;                    /* bits per pixel in this mode */
    u8                  banks;                  /* deprecated; total number of banks in this mode */
    u8                  memoryModel;
    u8                  bankSize;               /* deprecated; size of a bank, almost always 64 KB but may be 16 KB... */
    u8                  imagePages;
    u8                  __reserved_1__;

    u8                  redMask;
    u8                  redPosition;
    u8                  greenMask;
    u8                  greenPosition;
    u8                  blueMask;
    u8                  bluePosition;
    u8                  reservedMask;
    u8                  reservedPosition;
    u8                  directColorAttributes;

    u32                 lfbAddress;             /* phys. address of the linear frame buffer */
    u32                 offScreenMemAddr;
    u16                 offScreenMemSize;       /* size of of-screen framebuffer memory */

    u8                  padding[206];
} vesa_ModeInfo;

/* VESA BIOS Version Info */
typedef struct {
    u8          minor;
    u8          major;
} vesa_Version;

typedef struct {
    char                signature[4];           /* 'VESA' */
    vesa_Version        version;                /* e.g. 0x0300 for VBE 3.0 */
    char          _far *oemStringPtr;           /* FAR Pointer to OEM String */
    u8                  capabilities[4];        /* ?? */
    u16           _far *modeListPtr;            /* FAR Pointer to video mode list */
    u16                 totalMemory;            /* # of 64KiB blocks */
    u8                  padding[512 - 16];      /* Padding which USUALLY (not always!) contains the mode id list */
} vesa_BiosInfo;

#pragma pack()

bool vesa_getBiosInfo(vesa_BiosInfo *biosInfo);
bool vesa_isValidVesaBios(const vesa_BiosInfo *biosInfo);
/* Gets the amount of video modes this BIOS supports */
size_t vesa_getModeCount(const vesa_BiosInfo *biosInfo);
/*  Obtains VESA Mode information, modeId being the VESA Mode ID.
    Returns false if unsupported or no VESA BIOS. */
bool vesa_getModeInfoByModeId(vesa_ModeInfo *modeInfo, u16 modeId);
/*  Obtains VESA Mode information based on index of supported modes.
    i.e. returns the info for the index'th supported mode of this VBIOS.
    Returns false on error. */
bool vesa_getModeInfoByIndex(const vesa_BiosInfo *biosInfo, vesa_ModeInfo *modeInfo, size_t index);
/*  Gets total VRAM in bytes from the VESA BIOS info block. */
u32 vesa_getVRAMSize(const vesa_BiosInfo *biosInfo);

#endif
