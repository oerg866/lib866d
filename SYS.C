#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <dos.h>

#include "sys.h"
#include "types.h"
#include "386asm.h"
#include "util.h"

#define __LIB866D_TAG__ "SYS"
#include "debug.h"

#pragma pack(1)
/* Since we're lazy, we will just ignore the upper dword since we probably won't run on such a new system :-) */
typedef struct {
    struct { u32 low; u32 high; } base;
    struct { u32 low; u32 high; } length;
    u32 type; // entry Type
    u32 acpi; // extended
} sys_E820MemBlock;
#pragma pack()

static void swapE820Entries(sys_E820MemBlock *a, sys_E820MemBlock *b) {
    sys_E820MemBlock tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sortE820Entries(sys_E820MemBlock *regions, size_t regionCount) {
    size_t i;
    size_t j;

    /* I'm lazy so I used the simplest sorting code I could come up with :-)*/
    for (i = 0; i < regionCount; i++)
        for (j = i + 1; j < regionCount; j++)
            if (regions[i].base.low > regions[j].base.low) { swapE820Entries(&regions[i], &regions[j]); }
}

static void fixE820Overlaps(sys_E820MemBlock *regions, size_t regionCount) {
    size_t i;
    for (i = 0; i < regionCount - 1; i++) {
        /* Check if the section overlaps with the next */
        if ((regions[i].base.low + regions[i].length.low) > regions[i + 1].base.low) {
            /* if so, cap the length */
            regions[i].length.low = regions[i + 1].base.low - regions[i].base.low;
        }
    }
}

static sys_E820MemBlock *sys_getSortedInt15E820MemoryMap(size_t *entryCount) {
    sys_E820MemBlock       *regions         = NULL;
    sys_E820MemBlock  _far *curBlockFarPtr  = NULL;
    size_t                  regionCount     = 0;
    bool                    error           = false;

    u32                     tmp[3]          = { 0UL, 0UL, 0UL };
    u32               _far *magicFarPtr     = &tmp[0];
    u32               _far *entrySizeFarPtr = &tmp[1];
    u32               _far *blockIDFarPtr   = &tmp[2];

    do {
        regionCount++;
        regions = (sys_E820MemBlock *) realloc(regions, sizeof(sys_E820MemBlock) * regionCount);

        L866_NULLCHECK(regions);

        *entrySizeFarPtr = (u32) sizeof(sys_E820MemBlock);
        curBlockFarPtr = (sys_E820MemBlock _far*) &regions[regionCount-1];

        *magicFarPtr = 0x534D4150UL; /* 'SMAP' */

        _asm {
            PUSHAD
            MOV_REG_IMM(_EAX, 0x0000E820)
            MOV_REG_DWORDPTR(_EBX, blockIDFarPtr)
            MOV_REG_DWORDPTR(_ECX, entrySizeFarPtr)
            MOV_REG_DWORDPTR(_EDX, magicFarPtr)
            les di, curBlockFarPtr
            int 0x15

            MOV_DWORDPTR_REG(entrySizeFarPtr,   _ECX)
            MOV_DWORDPTR_REG(blockIDFarPtr,     _EBX)
            MOV_DWORDPTR_REG(magicFarPtr,       _EAX)

            jnc e820_noerr
            mov error, 1
        _ASM_LBL_(e820_noerr)
            POPAD
        }

        if (*magicFarPtr != 0x534D4150UL) {
            error = true;
            break;
        }

        DBG("E820 Region [%u] - address: %08lx length: %08lx, type %lx\n", (u16) regionCount-1,
            curBlockFarPtr->base.low, curBlockFarPtr->length.low, curBlockFarPtr->type);

    } while (error == false && *blockIDFarPtr != 0UL);

    if (error == true) {
        free(regions);
        *entryCount = 0;
        return NULL;
    }

    /* Now we need to sort them and fix overlapping sections */
    sortE820Entries(regions, regionCount);
    fixE820Overlaps(regions, regionCount);

    *entryCount = regionCount;
    return regions;
}

static u32 sys_getMemorySize_Int15E820Method(bool *hasMemoryHole) {
    bool found15MHole = false;
    u32 result = 0;
    u32 holeAddress = 0;
    u32 holeSize = 0;
    size_t regionCount = 0;
    size_t i;
    sys_E820MemBlock *regions = sys_getSortedInt15E820MemoryMap(&regionCount);

    DBG("E820 regions found: %u (buffer = 0x%p)\n", (u16) regionCount, regions);

    if (regions == NULL) {
        return 0UL;
    }

    for (i = 0; i < regionCount; i++) {
        DBG("E820 Region [%u] - address: %08lx length: %08lx, type %lx\n", (u16) i, regions[i].base.low, regions[i].length.low, regions[i].type);
        if (regions[i].base.low >= 1UL*1024UL*1024UL) { /* Find 1MB because we don't care about lower memory */
            /* If for some reason low memory has a bigger hole... */

            /* Check if this is a memory hole... */
            if (i > 0 && (regions[i-1].base.low + regions[i-1].length.low) < regions[i].base.low) {
                holeAddress = regions[i-1].base.low + regions[i-1].length.low;
                holeSize = regions[i].base.low - holeAddress;

                /* It might be the 15MB hole! */
                if ((holeAddress == 15UL * 1024UL * 1024UL) && (holeSize == 1UL * 1024UL * 1024UL)) {
                    DBG("16MB Memory hole found!\n");
                    found15MHole = true;
                    result += holeSize;
                    continue;
                } else {
                    DBG("Hole at region %u, address: %08lx length: %08lx, end of memory?\n", (u16) i, holeAddress, holeSize);
                    break;
                }
            }

            /* The hole might also manifest itself in a type 2 (reserved memory) region. */
            if ((regions[i].type == 2) && (regions[i].base.low == 15UL * 1024UL * 1024UL) && (regions[i].length.low == 1UL * 1024UL * 1024UL)) {
                    DBG("16MB Memory hole found!\n");
                    found15MHole = true;
                    /* no need to mess with the loop flow here, we'll count the size regularily */
            }

            result += regions[i].length.low;
        }
    }

    /* We skipped the first megabyte, so we need to add it. */
    if (result != 0) {
        result += 1UL*1024UL*1024UL;
    }

    if (hasMemoryHole != NULL) {
        *hasMemoryHole = found15MHole;
    }

    DBG("E820 total size: 0x%lx %lu\n", result, result);

    free(regions);
    return result;
}

static u32 sys_getMemorySize_Int15E801Method(bool *hasMemoryHole) {
    u32     result      = 0UL;
    u16     below16M    = 0;
    u16     above16M    = 0;
    bool    success     = false;

    _asm {
        xor cx, cx
        xor dx, dx
        xor bx, bx

        /* Int 0x15, Function 0xE801 */
        MOV_REG_IMM(_EAX, 0xE801)
        clc
        int 0x15

        /* Carry set = call failed */
        jc e801_error

        /* Unsuppoted Function */
        cmp ah, 0x86
        je e801_error

        /* if CX/DX are clear, use AX/BX instead */
        or cx, cx
        jz useaxbx

        /* use CX/DX */
        mov ax, cx
        mov bx, dx
    _ASM_LBL_(useaxbx)
        /* At this point ax = mem between 1M and 16M in K, BX = Mem above 16M in 64K Blocks */
        mov below16M, ax
        mov above16M, bx

        mov success, 1
    _ASM_LBL_(e801_error)
    }

    if (!success) {
        return 0;
    }

    /*  below16M = Mem between 1M and 16M in K
        above16M = Mem above 16M in 64K Blocks */

    below16M += 1024;

    if (hasMemoryHole != NULL && below16M == 15 * 1024) {
        *hasMemoryHole = true;
    } else {
        *hasMemoryHole = false;
    }

    /* For some reason above 16M counter is off by one...? */
    if (above16M) {
        above16M += 1;
    }

    result =  (u32) below16M * 1024UL;
    result += (u32) above16M * 64UL * 1024UL;
    return result;
}

u32 sys_getMemorySize(bool *hasMemoryHole) {
    u32 result;
    result = sys_getMemorySize_Int15E820Method(hasMemoryHole);

    /* Fallback method: Int 0x15, AX = 0xE801 */
    if (result == 0) {
        result = sys_getMemorySize_Int15E801Method(hasMemoryHole);
    }

    return result;
}

u32 sys_getPhysicalAddress(void _far *ptr) {
    u32 segment = (u32) FP_SEG(ptr);
    u32 offset = (u32) FP_OFF(ptr);
    return (segment << 4) + offset;
}

void sys_outPortL(u16 port, u32 outVal) {
    u32 _far *outValFarPtr = (u32 _far *) &outVal;
    UNUSED_ARG(outValFarPtr); /* asm macro below doesn't detect it as used */
    _asm {
        mov dx, port
        MOV_REG_DWORDPTR(_EAX, outValFarPtr)
        OUT_DX_EAX
    }
}

u32 sys_inPortL(u16 port) {
    u32 retVal = 0;
    u32 _far* retValFarPtr = &retVal;
    UNUSED_ARG(retValFarPtr); /* asm macro below doesn't detect it as used */
    _asm {
        mov dx, port
        IN_EAX_DX
        MOV_DWORDPTR_REG(retValFarPtr, _EAX)
    }
    return retVal;
}

void sys_ioDelay(u16 loops) {
    while (loops) {
        _asm {
            mov dx, 0xED
            out dx, al
        }
        loops--;
    }
}

sys_osWindowsMode sys_getWindowsMode(void) {
    u16 winMode = 0;

    /* WINDOWS Enhanced Mode Install Check (AX = 1600H) */
    _asm {
        mov ax, 0x1600
        int 0x2f
        mov winMode, ax
    }

    DBG("getWindowsMode: AX=%04x\n", winMode);

    switch (winMode) {
        case 0x0000:    return OS_PURE_DOS;
        case 0x1600:    return OS_PURE_DOS; /* DOS without XMS handler */
        case 0xFFFF:    return OS_WIN_REAL_MODE;
        case 0x0001:    return OS_WIN_STANDARD_MODE;
        case 0x0002:    return OS_WIN_ENHANCED_MODE;
        case 0x0004:    return OS_WIN_95;
        case 0x0A04:    return OS_WIN_98;
        case 0x5A04:    return OS_WIN_ME;
        default:        break;
    }
    return OS_UNKNOWN;
}

bool sys_allocateDMABuffer(sys_DMABuffer *buf, u32 size) {
    void _huge *raw;
    u32         rawSize;
    u32         rawPhys;
    u32         alignedPhys;
    u32         pageEnd;
    u16         alignedSegment;
    u16         alignedOffset;

    L866_NULLCHECK(buf);
    L866_ASSERTM(size <= 0x10000UL, "Requested buffer size out of range.");
    
    /* Allocate double so we are guaranteed to find a fitting
       aligned region within, regardless of where malloc lands */
    rawSize = size << 1;
    raw     = halloc(rawSize, 1);
    
    if (raw == NULL) {
        DBG("dma buffer alloc fail (%lu bytes)\n", size);
        return false;
    }

    /* Find the next 64K page boundary above rawPhys */
    rawPhys = sys_getPhysicalAddress(raw);
    pageEnd = (rawPhys & 0xFFFF0000UL) + 0x10000UL;

    /* If the raw buffer already fits before the page boundary,
    use it as-is, otherwise start after the boundary        */
    if (rawPhys + size <= pageEnd) {
        alignedPhys = rawPhys;
    } else {
        alignedPhys = pageEnd;
    }

    alignedSegment = (u16) (alignedPhys >> 4);
    alignedOffset  = (u16) (alignedPhys & 0xFUL);

    /* Convert flat physical address back to a normalised huge pointer
       seg  = alignedPhys >> 4
       off  = alignedPhys & 0x0F                                       */
    buf->aligned     = MK_FP(alignedSegment, alignedOffset);
    buf->alignedSize = (u16)size;
    buf->rawPtr      = raw;
    buf->rawSize     = rawSize;

    DBG("DMABuffer Alloc OK, aligned %08lx/%lp\n", alignedPhys, buf->aligned);

    return true;
}

void sys_freeDMABuffer(sys_DMABuffer *buf) {
    L866_NULLCHECK(buf);
    if (buf->rawPtr != NULL) {
        hfree(buf->rawPtr);
        buf->rawPtr      = NULL;
        buf->aligned     = NULL;
        buf->alignedSize = 0;
        buf->rawSize     = 0;
    }
}

bool sys_driveIsRemote(char letter) {
    /*  INT 21H AX=4409H: CHECK IF BLOCK DEVICE REMOTE
        BL = drive number (00h = default, 01h = A:, etc)
        Return: CF clear if successful
                DX = device attribute word */
    char drive = isupper(letter) ? letter - 'A' + 1 : letter - 'a' + 1;
    u8 error = 0;
    u16 attr = 0;

    _asm {
        mov ax, 0x4409
        mov bl, drive
        int 0x21
        jnc noErr
        mov error, 1
_ASM_LBL_(noErr)
        mov attr, dx
    };

    DBG("driveIsRemote INT 0x21 AX=4409 BL=%u %u %04x\n", (u16) drive, error, attr);

    if (error) return false;

    /* Bit 15 (SUBST), bit 12 (Remote) */
    if (attr & BIT(15) || attr & BIT(12)) return true;

    return false;
}
