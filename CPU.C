/*  LIB866D
    Low-Level CPU Functionality

    (C) 2026 E. Voirin (oerg866)
*/

#include "cpu.h"

#include <stdlib.h>

#include "386asm.h"
#include "util.h"

#define __LIB866D_TAG__ "CPU"
#include "debug.h"

#define CPU_RETURN_ON_NULL(ptr, return_value) if (ptr == NULL) { DBG("ERROR - '" #ptr "' is NULL! Result = '" #return_value "'\r\n"); return return_value; }

typedef struct {
    const char cpuidStr[13];
    cpu_Manufacturer mfr;
    const char *clearName;
} cpu_MfrLookupEntry;

static const cpu_MfrLookupEntry cpu_manufacturerTable[] = {
    { "AuthenticAMD", CPU_MFR_AMD,          "AMD"                       },
    { "CentaurHauls", CPU_MFR_IDT,          "IDT/Centaur"               },
    { "CyrixInstead", CPU_MFR_CYRIX,        "Cyrix/STM/IBM"             },
    { "GenuineIntel", CPU_MFR_INTEL,        "Intel"                     },
    { "GenuineIotel", CPU_MFR_INTEL,        "Intel"                     },
    { "TransmetaCPU", CPU_MFR_TRANSMETA,    "Transmeta"                 },
    { "GenuineTMx86", CPU_MFR_TRANSMETA,    "Transmeta"                 },
    { "Geode by NSC", CPU_MFR_NATSEMI,      "National Semiconductor"    },
    { "NexGenDriven", CPU_MFR_NEXGEN,       "NexGen"                    },
    { "RiseRiseRise", CPU_MFR_RISE,         "Rise"                      },
    { "SiS SiS SiS ", CPU_MFR_SIS,          "SiS"                       },
    { "UMC UMC UMC ", CPU_MFR_UMC,          "UMC"                       },
    { "Vortex86 SoC", CPU_MFR_DMP,          "DM&P"                      },
    { "  Shanghai  ", CPU_MFR_ZHAOXIN,      "Zaoxin"                    },
    { "HygonGenuine", CPU_MFR_HYGON,        "Hygon"                     },
    { "Genuine  RDC", CPU_MFR_RDC,          "RDC"                       },
    { "E2K MACHINE ", CPU_MFR_MCST,         "MCST Elbrus"               },
    { "VIA VIA VIA ", CPU_MFR_VIA,          "VIA"                       },
    { "AMD ISBETTER", CPU_MFR_AMDK5ES,      "AMD (K5 ES)"               },
    { "GenuineAO486", CPU_MFR_MISTER,       "MiSTer ao486"              },
    { "MiSTer AO486", CPU_MFR_MISTER,       "MiSTer ao486"              },
    { "MicrosoftXTA", CPU_MFR_MICROSOFT,    "Microsoft"                 },
    { "VirtualApple", CPU_MFR_APPLE,        "Apple"                     },
    { "            ", CPU_MFR_UNKNOWN,      "Unknown"                   }
};


bool cpu_getCPUIDString(char *outStr) {
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

cpu_CPUIDVersionInfo cpu_getCPUIDVersionInfo(void) {
    cpu_CPUIDVersionInfo result;
    void _far *resultPtr = (void _far *) &result;

    _asm {
        CPUID_LEVEL(1)
        les di, resultPtr
        MOV_DWORD_PTR_ESDI_OFFSET_REG(0, _EAX)
    }

    return result;
}

cpu_Manufacturer cpu_getManufacturer(const char **mfrClearName) {
    char cpuidStr[13] = { 0, };
    size_t mfrLookupIndex;

    if (cpu_getCPUIDString(cpuidStr) == false) {
        return CPU_MFR_UNKNOWN;
    }

    /* Find table entry for given CPUID string */
    for (mfrLookupIndex = 0; mfrLookupIndex < (size_t) ___CPU_MFR_COUNT___; mfrLookupIndex++) {
        if (0 == strcmp(cpuidStr, cpu_manufacturerTable[mfrLookupIndex].cpuidStr)) {
            if (mfrClearName != NULL) {
                *mfrClearName = cpu_manufacturerTable[mfrLookupIndex].clearName;
            }
            return cpu_manufacturerTable[mfrLookupIndex].mfr;
        }
    }

    /* No matching manufacturer found. You've got a rare CPU there! */
    return CPU_MFR_UNKNOWN;
}

bool cpu_readMSR(u32 msrId, cpu_MSR *msr) {
    u32 _far *msrFarPtr = (u32 _far *) msr;
    u32 _far *msrIdFarPtr = (u32 _far *) &msrId;

    UNUSED_ARG(msrId); /* asm macro below doesn't detect it as used */
    CPU_RETURN_ON_NULL(msr, false);

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

    DBG("cpu_readMSR: MSR 0x%08lx, eax = %08lx edx = %08lx\n", msrId, msr->lo, msr->hi);

    return true;
}

bool cpu_writeMSR(u32 msrId, const cpu_MSR *msr) {
    u32 _far *msrFarPtr = (u32 _far *) msr;
    u32 _far *msrIdFarPtr = (u32 _far *) &msrId;

    UNUSED_ARG(msrId); /* asm macro below doesn't detect it as used */
    CPU_RETURN_ON_NULL(msr, false);

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

    DBG("cpu_writeMSR: MSR 0x%08lx, eax = %08lx edx = %08lx\n", msrId, msr->lo, msr->hi);

    return true;
}

bool cpu_writeMSRAndVerify(u32 msrId, const cpu_MSR *msr) {
    cpu_MSR verify  = { 0UL, 0UL };
    bool    success = cpu_writeMSR(msrId, msr);

    success &= cpu_readMSR(msrId, &verify);
    success &= verify.lo == msr->lo;
    success &= verify.hi == msr->hi;

    return success;
}

bool cpu_readControlRegister(u8 index, u32 *out) {
    u32 _far *outFarPtr = (u32 _far *) out;

    CPU_RETURN_ON_NULL(out, false);
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

bool cpu_writeControlRegister(u8 index, const u32 *in) {
    u32 _far *inFarPtr = (u32 _far *) in;

    CPU_RETURN_ON_NULL(in, false);
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

bool cpu_isInV86Mode() {
    u8 result;
    __asm {
        SMSW_AX
        and ax, 1
        mov result, al
    }
    return result;
}