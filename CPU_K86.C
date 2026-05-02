/*  LIB866D
    AMD K86 Family CPU manipulation functions
    (K5, K6)

    (C) 2024 E. Voirin (oerg866)
*/

#include "cpu_k86.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "cpu.h"
#include "sys.h"
#include "types.h"
#include "386asm.h"
#include "util.h"

#define __LIB866D_TAG__ "CPU_K86.C"
#include "debug.h"

#define CPU_K86_MSR_EFER     0xC0000080UL   /* Extended Feature Enable Register (EFER) */
#define CPU_K86_MSR_WHCR     0xC0000082UL   /* Write Handling Control Register (WHCR) */
#define CPU_K86_MSR_UWCCR    0xC0000085UL   /* UC/WC Cachability Control Register (UWCCR) */
#define CPU_K86_MSR_EPMR     0xC0000086UL   /* Enhanced Power Management Register (EPMR) */

/* AMD K5 Write Allocate regs */
#define CPU_K86_MSR_WATMCR   0x00000085UL   /* Write Allocate Top-ot-Memory and Control Register (WATMCR) */
#define CPU_K86_MSR_WAPMRR   0x00000086UL   /* Write Allocate Programmable Memory Range Register (WAPMRR) */
#define CPU_K86_MSR_HWCR     0x00000083UL   /* Hardware Configuration Register (HWCR) */

#define CPU_K86_BADMUL       0xFF           /* Value indicating an invalid multiplier value */

#define _KB *1UL
#define _MB *1024UL _KB
#define _GB *1024UL _MB

static const u8 cpu_K86_setMultiplierValueTable[] = {
    CPU_K86_BADMUL, CPU_K86_BADMUL,         /* 0.0x, 0.5x (both invalid) */
    CPU_K86_BADMUL, CPU_K86_BADMUL,         /* 1.0x, 1.5x (both invalid) */
    0x04,           CPU_K86_BADMUL,         /* 2.0x, 2.5x (2.5x = invalid) */
    0x05,           0x07,                   /* 3.0x, 3.5x */
    0x02,           0x00,                   /* 4.0x, 4.5x */
    0x01,           0x03,                   /* 5.0x, 5.5x */
    0x06                                    /* 6.0x */
};

#define CPU_K86_MAX_MULTIPLIER_INDEX (ARRAY_SIZE(cpu_K86_setMultiplierValueTable) - 1)


/*  Returns whether or not the CPU's WHCR has the newer layout.
    Refer to AMD-K6®-2 Processor Data Sheet 21850J/0—February 2000
    Page 40, Write Handling Control Register (WHCR)–Model 8/[7:0] */
static bool cpu_K86_isNewWHCRLayout(void) {
    cpu_CPUIDVersionInfo cpuid = cpu_getCPUIDVersionInfo();
    u16 family = cpuid.basic.family;
    u16 model = cpuid.basic.model;
    u16 stepping = cpuid.basic.stepping;
    
    /* New layout is if it's a K6-2 with stepping 8 or higher (or a newer model) */

    return (family == 5 && model > 8)
        || (family == 5 && model == 8 && stepping >= 8);
}

/*  Returns whether the CPU is a K6 family CPU */
static bool cpu_K86_isK6Family(void) {
    cpu_CPUIDVersionInfo cpuid = cpu_getCPUIDVersionInfo();
    return (cpuid.basic.family == 5 && cpuid.basic.model >= 6);
}

/*  Returns whether the CPU is a K5 family CPU with Write Allocate */
static bool cpu_K86_isK5WithWriteAllocate(void) {
    cpu_CPUIDVersionInfo cpuid = cpu_getCPUIDVersionInfo();
    return (cpuid.basic.family == 5 && cpuid.basic.model <= 3 && cpuid.basic.stepping >= 4);
}

bool cpu_K86_enableEPMRIOBlock(bool enable) {
    u32 epmrBase = 0x0000FFF0 | (u32) enable; /* EPMR Base + Enable bit */
    cpu_MSR msr;
    msr.lo = epmrBase;
    msr.hi = 0UL;
    return cpu_writeMSR(CPU_K86_MSR_EPMR, &msr);
}

cpu_K86_SetMulError cpu_K86_setMultiplier(u16 whole, u16 fraction) {
    size_t multiIndex;
    u32 multiplierValue = CPU_K86_BADMUL;

    if ((fraction != 0 && fraction != 5) || whole > 6) {
        return SETMUL_BADMUL;
    }

    multiIndex = whole * 2 + fraction / 5;

    if (multiIndex > CPU_K86_MAX_MULTIPLIER_INDEX) {
        return SETMUL_BADMUL;
    }

    multiplierValue = (u32) cpu_K86_setMultiplierValueTable[multiIndex];

    DBG("setMultiplier: Multiplier value: 0x%04lx\n", multiplierValue);

    if (multiplierValue == CPU_K86_BADMUL){
        DBG("setMultiplier: Invalid multiplier!\n", multiplierValue);
        return SETMUL_BADMUL;
    }

    if (cpu_K86_enableEPMRIOBlock(true) == false) {
        return SETMUL_ERROR;
    }

    /* Prepare multiplier value and shift to fill 'Internal Bus Divisor' Field */
    multiplierValue &= 0x00000007UL;
    multiplierValue <<= 5UL;
    /* Set counter to force CPU into EPM Stop Grant State (to apply values)
        Refer to AMD-K6-2E+ Embedded Processor Data Sheet
        23542A/0 — September 2000, Page 147 */
    multiplierValue |= 0x00001000UL;
    /* Set bus divisor control to 10b to cause the
        IBF field to be sampled when entering EPM Stop Grant State */
    multiplierValue |= 0x00000200UL;
    /* Output value to Bus Divisor and Voltage ID Control (BVC)
        (IOBASE + 0x08) */

    DBG("setMultiplier: Encoded BVC value: %08lx\n", multiplierValue);

    sys_outPortL(0xFFF8, multiplierValue);

    return cpu_K86_enableEPMRIOBlock(false) ? SETMUL_OK : SETMUL_ERROR;
}

bool cpu_K86_setWriteOrderMode(cpu_K86_WriteOrderMode mode) {
    /* prepare the EWBEC bits with the word supplied in mode */
    u32     modeBits = ((u32) mode << 2) & 0x0000000CUL;
    cpu_MSR msr;
    bool    success;

    if (mode >= __CPU_K86_WRITEORDER_MODE_COUNT__) {
        return false;
    }

    /* Read EFER to manipulate it */
    success = cpu_readMSR(CPU_K86_MSR_EFER, &msr);
    /* Mask the EWBEC bits 2 and 3. It's also important that we
       do not fault the CPU by writing reserved bits */
    msr.lo &= 0x000000F3UL;
    msr.lo |= modeBits;
    /* Write new EFER to MSR */
    success &= cpu_writeMSR(CPU_K86_MSR_EFER, &msr);

    return success;
}

bool cpu_K86_setWriteAllocateRange(const cpu_K86_WriteAllocateConfig *config) {
    if (config == NULL) {
        return false;
    }

    return cpu_K86_setWriteAllocateRangeValues(config->sizeKB, config->memoryHole);
}


static bool cpu_K86_setWriteAllocate_K6_2(u32 sizeKB, bool memoryHole) {
    cpu_MSR msr;
    /* Mask Write Allocate range bits (K6-2 or higher)*/
    msr.lo = (sizeKB * 1024UL) & 0xFFC00000UL;
    msr.lo |= (u32) memoryHole << 16UL;
    msr.hi = 0UL;
    return cpu_writeMSRAndVerify(CPU_K86_MSR_WHCR, &msr);
}

static bool cpu_K86_setWriteAllocate_K6(u32 sizeKB, bool memoryHole) {
    cpu_MSR msr;
    /* Regular K6 and early K6-2 has a different layout */
    if (sizeKB > (508UL _MB)) {
        DBG("Write allocate size out of range.\n");
        return false;
    }
    /* WAELIM field is amount of 4MB blocks to cover */
    msr.lo = ((sizeKB / 1024UL) / 4UL) << 1;
    msr.lo |= (u32) memoryHole;
    msr.hi = 0UL;
    return cpu_writeMSRAndVerify(CPU_K86_MSR_WHCR, &msr);
}

static bool cpu_K86_setWriteAllocate_K5(u32 sizeKB, bool memoryHole) {
    cpu_MSR wapmrr;
    cpu_MSR watmcr;
    cpu_MSR hwcr;
   
    /*
        Reference:
        Implementation of Write Allocate in the K86™ Processors 21326A/O-March 1997
        Page 2-7
     */

    if (sizeKB > (2 _GB)) {
        DBG("Write allocate size out of range.\n");
        return false;
    }

    /* Disable Write Allocate Bit */
    if (!cpu_readMSR(CPU_K86_MSR_HWCR, &hwcr)) return false;
    hwcr.lo &= ~(0x00000010UL);
    if (!cpu_writeMSRAndVerify(CPU_K86_MSR_HWCR, &hwcr)) return false;

    /* Set Write Allocate Region */
    wapmrr.hi = 0UL;
    /*  "from" = 0x0001000, "to" = <top of mem> */
    wapmrr.lo = (((sizeKB - 1UL) * 1024UL) & 0xFFFF0000);
    wapmrr.lo |= 0x0001UL;
    if (!cpu_writeMSRAndVerify(CPU_K86_MSR_WAPMRR, &wapmrr)) return false;

    /* Set Top Of Memory, protect top of memory + fixed range (a0000 - fffff) from WA */
    watmcr.hi = 0UL;
    watmcr.lo = (sizeKB * 1024UL) >> 16;
    watmcr.lo |= 0x00000400UL;
    watmcr.lo |= 0x00000100UL;
    if (!cpu_writeMSRAndVerify(CPU_K86_MSR_WATMCR, &watmcr)) return false;

    /* Enable Write Allocate Bit */
    if (!cpu_readMSR(CPU_K86_MSR_HWCR, &hwcr)) return false;
    hwcr.lo |= 0x00000010UL;
    if (!cpu_writeMSRAndVerify(CPU_K86_MSR_HWCR, &hwcr)) return false;

    return true;
}

bool cpu_K86_setWriteAllocateRangeValues(u32 sizeKB, bool memoryHole) {
    cpu_MSR msr;

    if (cpu_K86_isNewWHCRLayout()) {
        return cpu_K86_setWriteAllocate_K6_2(sizeKB, memoryHole);
    } else if (cpu_K86_isK6Family()) {
        return cpu_K86_setWriteAllocate_K6(sizeKB, memoryHole);
    } else if (cpu_K86_isK5WithWriteAllocate()) {
        return cpu_K86_setWriteAllocate_K5(sizeKB, memoryHole);
    } else {
        DBG("Unsupported CPU\n");
        return false;
    }
}

static bool cpu_K86_getWriteAllocateRange_K6_2(cpu_K86_WriteAllocateConfig *config) {
    cpu_MSR msr;
    if (cpu_readMSR(CPU_K86_MSR_WHCR, &msr) == false) return false;
    config->sizeKB = (msr.lo & 0xFFC00000UL) / 1024UL;
    config->memoryHole = (msr.lo >> 5UL) ? true : false;
    return true;
}

static bool cpu_K86_getWriteAllocateRange_K6(cpu_K86_WriteAllocateConfig *config) {
    cpu_MSR msr;
    u32 blocks = (msr.lo & 0xFFUL) >> 1;
    if (cpu_readMSR(CPU_K86_MSR_WHCR, &msr) == false) return false;
    config->sizeKB = blocks * 4UL * 1024UL;
    config->memoryHole = (msr.lo & 0x01UL) ? true : false;
    return true;
}

/* Get Write Allocate range for K5 CPUs */
static bool cpu_K86_getWriteAllocateRange_K5(cpu_K86_WriteAllocateConfig *config) {
    cpu_MSR wapmrr;
    cpu_MSR watmcr;
    cpu_MSR hwcr;

    /* Get Write Allocate Region */
    if (!cpu_readMSR(CPU_K86_MSR_WAPMRR, &wapmrr)) return false;

    /*  Upper WORD is the upper WORD of the TOP, and the logic for "to" makes it until ....FFFF 
        Add 1 to that and you get size */
    config->sizeKB = (((wapmrr.lo & 0xFFFFUL) | 0xFFFFUL) + 1UL) / 1024UL;

    /*  to my knowledge, this doesn't support the hole */
    config->memoryHole = false;

    return true;
}

bool cpu_K86_getWriteAllocateRange(cpu_K86_WriteAllocateConfig *config) {
    cpu_MSR msr;

    if (config == NULL) return false;

    if (cpu_K86_isNewWHCRLayout()) {
        return cpu_K86_getWriteAllocateRange_K6_2(config);
    } else if (cpu_K86_isK6Family()) {
        return cpu_K86_getWriteAllocateRange_K6(config);
    } else if (cpu_K86_isK5WithWriteAllocate()) {
        return cpu_K86_getWriteAllocateRange_K5(config);
    } else {
        DBG("Unsupported CPU\n");
        return false;
    }
}

/* MTRR Mask look up table */
typedef struct {
    u32 mask;
    u32 sizeKB;
} MTRRMask;
static const MTRRMask cpu_K86_mtrrMaskTable[] = {
    { (0x7FFFUL << 0)  & 0x7FFFUL, 128 _KB },
    { (0x7FFFUL << 1)  & 0x7FFFUL, 256 _KB },
    { (0x7FFFUL << 2)  & 0x7FFFUL, 512 _KB },
    { (0x7FFFUL << 3)  & 0x7FFFUL,   1 _MB },
    { (0x7FFFUL << 4)  & 0x7FFFUL,   2 _MB },
    { (0x7FFFUL << 5)  & 0x7FFFUL,   4 _MB },
    { (0x7FFFUL << 6)  & 0x7FFFUL,   8 _MB },
    { (0x7FFFUL << 7)  & 0x7FFFUL,  16 _MB },
    { (0x7FFFUL << 8)  & 0x7FFFUL,  32 _MB },
    { (0x7FFFUL << 9)  & 0x7FFFUL,  64 _MB },
    { (0x7FFFUL << 10) & 0x7FFFUL, 128 _MB },
    { (0x7FFFUL << 11) & 0x7FFFUL, 256 _MB },
    { (0x7FFFUL << 12) & 0x7FFFUL, 512 _MB },
    { (0x7FFFUL << 13) & 0x7FFFUL,   1 _GB },
    { (0x7FFFUL << 14) & 0x7FFFUL,   2 _GB },
    { (0x7FFFUL << 15) & 0x7FFFUL,   4 _GB },
};

#define MTRR_MASK_COUNT ARRAY_SIZE(cpu_K86_mtrrMaskTable)

static u32 cpu_K86_getBestMTTRMaskFromSizeKB(u32 sizeKB) {
    size_t i;
    for (i = 0; i < MTRR_MASK_COUNT; i++) {
        if (cpu_K86_mtrrMaskTable[i].sizeKB >= sizeKB)
            break;
    }

    if (i == MTRR_MASK_COUNT) {
        return 0;
    }

    DBG("MTRR: Selecting mask 0x%08lx for %lu KB\n", cpu_K86_mtrrMaskTable[i].mask, sizeKB);

    return cpu_K86_mtrrMaskTable[i].mask;
}

#define K6_MTRR_IS_WC(msr)  ((bool) ((msr & 0x02UL) >> 1))
#define K6_MTRR_IS_UC(msr)  ((bool) ((msr & 0x01UL) >> 1))
#define K6_MTRR_OFFSET(msr) (msr & 0xFFFE0000UL)
#define K6_MTRR_MASK(msr)   ((msr & 0x00001FFFCUL) >> 2)

/* Returns FALSE if mask is invalid. */
static bool cpu_K86_getSizeKBFromMTRRMask(u32 mask, u32 *lengthOut) {
    size_t i;
    DBG("cpu_K86_getSizeKBFromMTRRMask: mask %08lx\n", mask);
    for (i = 0; i < MTRR_MASK_COUNT; i++) {
            if (cpu_K86_mtrrMaskTable[i].mask == mask) {
                *lengthOut = cpu_K86_mtrrMaskTable[i].sizeKB;
                return true;
            }
    }
    *lengthOut = 0UL;
    return false;
}

static void cpu_K86_decodeMTRRs(cpu_K86_MemoryTypeRangeRegs *mtrr, const cpu_MSR *msr) {
    mtrr->configs[0].offset         = K6_MTRR_OFFSET(msr->lo);
    mtrr->configs[0].uncacheable    = K6_MTRR_IS_UC(msr->lo);
    mtrr->configs[0].writeCombine   = K6_MTRR_IS_WC(msr->lo);
    mtrr->configs[0].isValid        = cpu_K86_getSizeKBFromMTRRMask(K6_MTRR_MASK(msr->lo), &mtrr->configs[0].sizeKB);
    mtrr->configs[0].isValid       &= (msr->lo != 0UL);

    mtrr->configs[1].offset         = K6_MTRR_OFFSET(msr->hi);
    mtrr->configs[1].uncacheable    = K6_MTRR_IS_UC(msr->hi);
    mtrr->configs[1].writeCombine   = K6_MTRR_IS_WC(msr->hi);
    mtrr->configs[1].isValid        = cpu_K86_getSizeKBFromMTRRMask(K6_MTRR_MASK(msr->hi), &mtrr->configs[1].sizeKB);
    mtrr->configs[1].isValid       &= (msr->hi != 0UL);
}

bool cpu_K86_getMemoryTypeRanges(cpu_K86_MemoryTypeRangeRegs *regs) {
    cpu_MSR msr;
    bool success = cpu_readMSR(CPU_K86_MSR_UWCCR, &msr);
    L866_NULLCHECK(regs);

    if (!success) {
        return false;
    }

    cpu_K86_decodeMTRRs(regs, &msr);
    return true;
}

static void cpu_K86_encodeMTRRs(cpu_MSR *msr, const cpu_K86_MemoryTypeRangeRegs *mtrr) {
    memset(msr, 0, sizeof(cpu_MSR));

    if (mtrr->configs[0].isValid)
        msr->lo = (mtrr->configs[0].offset & 0xFFFE0000UL)
            | (cpu_K86_getBestMTTRMaskFromSizeKB(mtrr->configs[0].sizeKB) << 2UL)
            | (mtrr->configs[0].writeCombine ? 0x02UL : 0x00UL)
            | (mtrr->configs[0].uncacheable ? 0x01UL : 0x00UL);

    if (mtrr->configs[1].isValid)
        msr->hi = (mtrr->configs[1].offset & 0xFFFE0000UL)
            | (cpu_K86_getBestMTTRMaskFromSizeKB(mtrr->configs[1].sizeKB) << 2UL)
            | (mtrr->configs[1].writeCombine ? 0x02UL : 0x00UL)
            | (mtrr->configs[1].uncacheable ? 0x01UL : 0x00UL);

    DBG("cpu_K86_encodeMTRRs: [0x%08lx, 0x%08lx]\n", msr->lo, msr->hi);
}

bool cpu_K86_setMemoryTypeRanges(const cpu_K86_MemoryTypeRangeRegs *regs) {
    cpu_MSR  msr;

    L866_NULLCHECK(regs);
    cpu_K86_encodeMTRRs(&msr, regs);
    return cpu_writeMSRAndVerify(CPU_K86_MSR_UWCCR, &msr);
}

bool cpu_K86_setL1Cache(bool enable) {
    bool success = true;
    u32 cr0;

    success &= cpu_readControlRegister(0, &cr0);
    cr0 &= 0x9FFFFFFFUL; /* Mask Cache Disable + Non-Writeback */
    cr0 |= ((enable) ? 0UL : 0x60000000UL );
    success &= cpu_writeControlRegister(0, &cr0);
    return success;
}

bool cpu_K86_setL2Cache(bool enable) {
    cpu_MSR msr;
    bool    success = true;

    success &= cpu_readMSR(CPU_K86_MSR_EFER, &msr);
    msr.lo &= 0xFFFFFFEFUL; /* Mask L2 Disable */
    msr.lo |= ((enable) ? 0UL : 0x00000010UL);
    success &= cpu_writeMSR(CPU_K86_MSR_EFER, &msr);
    return success;
}

bool cpu_K86_getL1CacheStatus(void) {
    u32     cr0 = 0;
    bool    success = cpu_readControlRegister(0, &cr0);
 
    L866_ASSERT(success);
    return (cr0 & 0x40000000UL) == 0UL;
}

bool cpu_K86_getL2CacheStatus(void) {
    cpu_MSR msr;
    bool    success = cpu_readMSR(CPU_K86_MSR_EFER, &msr);

    L866_ASSERT(success);
    return (msr.lo & 0x00000010UL) == 0UL;
}

bool cpu_K86_setDataPrefetch(bool enable) {
    cpu_MSR msr;
    bool    success = true;

    success &= cpu_readMSR(CPU_K86_MSR_EFER, &msr);
    msr.lo &= 0xFFFFFFFDUL; /* Mask Data Prefetch Enable */
    msr.lo |= ((enable) ? 0x00000002UL : 0);
    success &= cpu_writeMSR(CPU_K86_MSR_EFER, &msr);
    return success;
}
