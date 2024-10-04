#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "sys.h"
#include "types.h"
#include "386asm.h"
#include "util.h"

#define __LIB866D_TAG__ "SYS"
#include "debug.h"

#define SYS_RETURN_ON_NULL(ptr, return_value) if (ptr == NULL) { DBG("ERROR - '" #ptr "' is NULL! Result = '" #return_value "'\r\n"); return return_value; }

typedef struct {
    const char cpuidStr[13];
    sys_CPUManufacturer mfr;
    const char *clearName;
} sys_CPUMfrLookupEntry;

static const sys_CPUMfrLookupEntry sys_cpuManufacturerTable[] = {
    { "AuthenticAMD", SYS_CPU_MFR_AMD,          "AMD"                       },
    { "CentaurHauls", SYS_CPU_MFR_IDT,          "IDT/Centaur"               },
    { "CentaurHauls", SYS_CPU_MFR_CYRIX,        "Cyrix/STM/IBM"             },
    { "GenuineIntel", SYS_CPU_MFR_INTEL,        "Intel"                     },
    { "GenuineIotel", SYS_CPU_MFR_INTEL,        "Intel"                     },
    { "TransmetaCPU", SYS_CPU_MFR_TRANSMETA,    "Transmeta"                 },
    { "GenuineTMx86", SYS_CPU_MFR_TRANSMETA,    "Transmeta"                 },
    { "Geode by NSC", SYS_CPU_MFR_NATSEMI,      "National Semiconductor"    },
    { "NexGenDriven", SYS_CPU_MFR_NEXGEN,       "NexGen"                    },
    { "RiseRiseRise", SYS_CPU_MFR_RISE,         "Rise"                      },
    { "SiS SiS SiS ", SYS_CPU_MFR_SIS,          "SiS"                       },
    { "UMC UMC UMC ", SYS_CPU_MFR_UMC,          "UMC"                       },
    { "Vortex86 SoC", SYS_CPU_MFR_DMP,          "DM&P"                      },
    { "  Shanghai  ", SYS_CPU_MFR_ZHAOXIN,      "Zaoxin"                    },
    { "HygonGenuine", SYS_CPU_MFR_HYGON,        "Hygon"                     },
    { "Genuine  RDC", SYS_CPU_MFR_RDC,          "RDC"                       },
    { "E2K MACHINE ", SYS_CPU_MFR_MCST,         "MCST Elbrus"               },
    { "VIA VIA VIA ", SYS_CPU_MFR_VIA,          "VIA"                       },
    { "AMD ISBETTER", SYS_CPU_MFR_AMDK5ES,      "AMD (K5 ES)"               },
    { "GenuineAO486", SYS_CPU_MFR_MISTER,       "MiSTer ao486"              },
    { "MiSTer AO486", SYS_CPU_MFR_MISTER,       "MiSTer ao486"              },
    { "MicrosoftXTA", SYS_CPU_MFR_MICROSOFT,    "Microsoft"                 },
    { "VirtualApple", SYS_CPU_MFR_APPLE,        "Apple"                     },
    { "            ", SYS_CPU_MFR_UNKNOWN,      "Unknown"                   }
};

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
    memcpy(&tmp,    a,      sizeof(sys_E820MemBlock));
    memcpy(a,       b,      sizeof(sys_E820MemBlock));
    memcpy(b,       &tmp,   sizeof(sys_E820MemBlock));
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
        free(regions);
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

bool sys_getCPUIDString(char *outStr) {
    /* TODO: Error out if CPU does not support CPUID. */
    char _far *outStrFar = (char _far *) outStr;
    outStr[12] = 0x00;

    _asm {
        CPUID_LEVEL(0)
        les di, outStrFar
        MOV_DWORD_PTR_ESDI_OFFSET_REG(0, _EBX)
        MOV_DWORD_PTR_ESDI_OFFSET_REG(4, _EDX)
        MOV_DWORD_PTR_ESDI_OFFSET_REG(8, _ECX)
    }

    return true;
}

sys_CPUIDVersionInfo sys_getCPUIDVersionInfo(void) {
    sys_CPUIDVersionInfo result;
    void _far *resultPtr = (void _far *) &result;

    _asm {
        CPUID_LEVEL(1)
        les di, resultPtr
        MOV_DWORD_PTR_ESDI_OFFSET_REG(0, _EAX)
    }

    return result;
}

sys_CPUManufacturer sys_getCPUManufacturer(const char **mfrClearName) {
    char cpuidStr[13] = { 0, };
    size_t mfrLookupIndex;

    if (sys_getCPUIDString(cpuidStr) == false) {
        return SYS_CPU_MFR_UNKNOWN;
    }

    /* Find table entry for given CPUID string */
    for (mfrLookupIndex = 0; mfrLookupIndex < (size_t) ___SYS_CPU_MFR_COUNT___; mfrLookupIndex++) {
        if (0 == strcmp(cpuidStr, sys_cpuManufacturerTable[mfrLookupIndex].cpuidStr)) {
            if (mfrClearName != NULL) {
                *mfrClearName = sys_cpuManufacturerTable[mfrLookupIndex].clearName;
            }
            return sys_cpuManufacturerTable[mfrLookupIndex].mfr;
        }
    }

    /* No matching manufacturer found. You've got a rare CPU there! */
    return SYS_CPU_MFR_UNKNOWN;
}

bool sys_cpuReadMSR(u32 msrId, sys_CPUMSR *msr) {
    u32 _far *msrFarPtr = (u32 _far *) msr;
    u32 _far *msrIdFarPtr = (u32 _far *) &msrId;

    UNUSED_ARG(msrId); /* asm macro below doesn't detect it as used */
    SYS_RETURN_ON_NULL(msr, false);

    _asm {
        pushf
        cli
        WBINVD
        MOV_REG_DWORDPTR(_ECX, msrIdFarPtr)
        RDMSR
        /* CPUMSR are two packed DWORDS so we can access them like this */
        les di, dword ptr msrFarPtr
        MOV_DWORD_PTR_ESDI_OFFSET_REG(0, _EAX)
        MOV_DWORD_PTR_ESDI_OFFSET_REG(4, _EDX)
        popf
    }

    DBG("sys_cpuReadMSR: MSR 0x%08lx, eax = %08lx edx = %08lx\n", msrId, msr->lo, msr->hi);

    return true;
}

bool sys_cpuWriteMSR(u32 msrId, const sys_CPUMSR *msr) {
    u32 _far *msrFarPtr = (u32 _far *) msr;
    u32 _far *msrIdFarPtr = (u32 _far *) &msrId;

    UNUSED_ARG(msrId); /* asm macro below doesn't detect it as used */
    SYS_RETURN_ON_NULL(msr, false);

    _asm {
        pushf
        cli
        WBINVD
        MOV_REG_DWORDPTR(_ECX, msrIdFarPtr)
        /* CPUMSR are two packed DWORDS so we can access them like this */
        les di, dword ptr msrFarPtr
        MOV_REG_DWORD_PTR_ESDI_OFFSET(_EAX, 0)
        MOV_REG_DWORD_PTR_ESDI_OFFSET(_EDX, 4)
        WRMSR
        popf
    }

    DBG("sys_cpuWriteMSR: MSR 0x%08lx, eax = %08lx edx = %08lx\n", msrId, msr->lo, msr->hi);

    return true;
}

bool sys_cpuWriteMSRAndVerify(u32 msrId, const sys_CPUMSR *msr) {
    sys_CPUMSR  verify  = { 0UL, 0UL };
    bool        success = sys_cpuWriteMSR(msrId, msr);

    success &= sys_cpuReadMSR(msrId, &verify);
    success &= verify.lo == msr->lo;
    success &= verify.hi == msr->hi;

    return success;
}

bool sys_cpuReadControlRegister(u8 index, u32 *out) {
    u32 _far *outFarPtr = (u32 _far *) out;

    SYS_RETURN_ON_NULL(out, false);
    if (index >= 8) {
        return false;
    }

    /* Sorry this is really ugly... */
    switch (index) {
        case 0: _asm { MOV_DWORD_PTR_CR(0, outFarPtr) }; break;
        case 1: _asm { MOV_DWORD_PTR_CR(1, outFarPtr) }; break;
        case 2: _asm { MOV_DWORD_PTR_CR(2, outFarPtr) }; break;
        case 3: _asm { MOV_DWORD_PTR_CR(3, outFarPtr) }; break;
        case 4: _asm { MOV_DWORD_PTR_CR(4, outFarPtr) }; break;
        case 5: _asm { MOV_DWORD_PTR_CR(5, outFarPtr) }; break;
        case 6: _asm { MOV_DWORD_PTR_CR(6, outFarPtr) }; break;
        case 7: _asm { MOV_DWORD_PTR_CR(7, outFarPtr) }; break;
        default: return false;
    }

    DBG("Read CR%u: 0x%08lx\n", (u16) index, *out);
    return true;
}

bool sys_cpuWriteControlRegister(u8 index, const u32 *in) {
    u32 _far *inFarPtr = (u32 _far *) in;

    SYS_RETURN_ON_NULL(in, false);
    if (index >= 8) {
        return false;
    }

    /* Sorry this is really ugly... */
    switch (index) {
        case 0: _asm { MOV_CR_DWORD_PTR(0, inFarPtr) }; break;
        case 1: _asm { MOV_CR_DWORD_PTR(1, inFarPtr) }; break;
        case 2: _asm { MOV_CR_DWORD_PTR(2, inFarPtr) }; break;
        case 3: _asm { MOV_CR_DWORD_PTR(3, inFarPtr) }; break;
        case 4: _asm { MOV_CR_DWORD_PTR(4, inFarPtr) }; break;
        case 5: _asm { MOV_CR_DWORD_PTR(5, inFarPtr) }; break;
        case 6: _asm { MOV_CR_DWORD_PTR(6, inFarPtr) }; break;
        case 7: _asm { MOV_CR_DWORD_PTR(7, inFarPtr) }; break;
        default: return false;
    }

    DBG("Write CR%u: 0x%08lx\n", (u16) index, *in);
    return true;
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
