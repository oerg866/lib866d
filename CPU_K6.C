/*  LIB866D
    AMD K6 Family CPU manipulation functions

    (C) 2024 E. Voirin (oerg866)
*/

#include "cpu_k6.h"

#include <stddef.h>
#include <string.h>

#include "sys.h"
#include "types.h"
#include "386asm.h"
#include "util.h"

#define __LIB866D_TAG__ "CPU_K6.C"
#include "debug.h"

#define CPU_K6_MSR_EFER     0xC0000080UL    /* Extended Feature Enable Register (EFER) */
#define CPU_K6_MSR_WHCR     0xC0000082UL    /* Write Handling Control Register (WHCR) */
#define CPU_K6_MSR_UWCCR    0xC0000085UL    /* UC/WC Cachability Control Register (UWCCR) */
#define CPU_K6_MSR_EPMR     0xC0000086UL    /* Enhanced Power Management Register (EPMR) */

#define CPU_K6_BADMUL       0xFF            /* Value indicating an invalid multiplier value */

static const u8 cpu_K6_setMultiplierValueTable[] = {
    CPU_K6_BADMUL,  CPU_K6_BADMUL,          /* 0.0x, 0.5x (both invalid) */
    CPU_K6_BADMUL,  CPU_K6_BADMUL,          /* 1.0x, 1.5x (both invalid) */
    0x04,           CPU_K6_BADMUL,          /* 2.0x, 2.5x (2.5x = invalid) */
    0x05,           0x07,                   /* 3.0x, 3.5x */
    0x02,           0x00,                   /* 4.0x, 4.5x */
    0x01,           0x03,                   /* 5.0x, 5.5x */
    0x06                                    /* 6.0x */
};

#define CPU_K6_MAX_MULTIPLIER_INDEX (ARRAY_SIZE(cpu_K6_setMultiplierValueTable) - 1)

bool cpu_K6_enableEPMRIOBlock(bool enable) {
    u32 epmrBase = 0x0000FFF0 | (u32) enable; /* EPMR Base + Enable bit */
    sys_CPUMSR msr;
    msr.lo = epmrBase;
    msr.hi = 0UL;
    return sys_cpuWriteMSR(CPU_K6_MSR_EPMR, &msr);
}

cpu_K6_SetMulError cpu_K6_setMultiplier(u16 whole, u16 fraction) {
    size_t multiIndex;
    u32 multiplierValue = CPU_K6_BADMUL;

    if ((fraction != 0 && fraction != 5) || whole > 6) {
        return SETMUL_BADMUL;
    }

    multiIndex = whole * 2 + fraction / 5;

    if (multiIndex > CPU_K6_MAX_MULTIPLIER_INDEX) {
        return SETMUL_BADMUL;
    }

    multiplierValue = (u32) cpu_K6_setMultiplierValueTable[multiIndex];

    DBG("setMultiplier: Multiplier value: 0x%04lx\n", multiplierValue);

    if (multiplierValue == CPU_K6_BADMUL){
        DBG("setMultiplier: Invalid multiplier!\n", multiplierValue);
        return SETMUL_BADMUL;
    }

    if (cpu_K6_enableEPMRIOBlock(true) == false) {
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

    return cpu_K6_enableEPMRIOBlock(false) ? SETMUL_OK : SETMUL_ERROR;
}

bool cpu_K6_setWriteOrderMode(cpu_K6_WriteOrderMode mode) {
    /* prepare the EWBEC bits with the word supplied in mode */
    u32         modeBits = ((u32) mode << 2) & 0x0000000CUL;
    sys_CPUMSR  msr;
    bool        success;

    if (mode >= __CPU_K6_WRITEORDER_MODE_COUNT__) {
        return false;
    }

    /* Read EFER to manipulate it */
    success = sys_cpuReadMSR(CPU_K6_MSR_EFER, &msr);
    /* Mask the EWBEC bits 2 and 3. It's also important that we
       do not fault the CPU by writing reserved bits */
    msr.lo &= 0x000000F3UL;
    msr.lo |= modeBits;
    /* Write new EFER to MSR */
    success &= sys_cpuWriteMSR(CPU_K6_MSR_EFER, &msr);

    return success;
}

bool cpu_K6_setWriteAllocateRange(const cpu_K6_WriteAllocateConfig *config) {
    if (config == NULL) {
        return false;
    }

    return cpu_K6_setWriteAllocateRangeValues(config->sizeKB, config->memoryHole);
}

bool cpu_K6_setWriteAllocateRangeValues(u32 sizeKB, bool memoryHole) {
    sys_CPUMSR msr;

    /* Mask Write Allocate range bits */
    msr.lo = (sizeKB * 1024UL) & 0xFFC00000UL;
    msr.lo |= (u32) memoryHole << 5UL;
    msr.hi = 0UL;

    return sys_cpuWriteMSRAndVerify(CPU_K6_MSR_WHCR, &msr);
}

bool cpu_K6_getWriteAllocateRange(cpu_K6_WriteAllocateConfig *config) {
    sys_CPUMSR  msr;

    if (config == NULL) {
        return false;
    }

    if (sys_cpuReadMSR(CPU_K6_MSR_WHCR, &msr) == false) {
        return false;
    }

    config->sizeKB = (msr.lo & 0xFFC00000UL) / 1024UL;
    config->memoryHole = (msr.lo >> 5UL) ? true : false;
    return true;
}

/* MTRR Mask look up table */
typedef struct {
    u32 mask;
    u32 sizeKB;
} MTRRMask;
#define _KB *1
#define _MB *1024UL _KB
#define _GB *1024UL _MB
static const MTRRMask cpu_K6_mtrrMaskTable[] = {
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

#define MTRR_MASK_COUNT ARRAY_SIZE(cpu_K6_mtrrMaskTable)

static u32 cpu_K6_getBestMTTRMaskFromSizeKB(u32 sizeKB) {
    size_t i;
    for (i = 0; i < MTRR_MASK_COUNT; i++) {
        if (cpu_K6_mtrrMaskTable[i].sizeKB >= sizeKB)
            break;
    }

    if (i == MTRR_MASK_COUNT) {
        return 0;
    }

    DBG("MTRR: Selecting mask 0x%08lx for %lu KB\n", cpu_K6_mtrrMaskTable[i].mask, sizeKB);

    return cpu_K6_mtrrMaskTable[i].mask;
}

#define K6_MTRR_IS_WC(msr)  ((bool) ((msr & 0x02UL) >> 1))
#define K6_MTRR_IS_UC(msr)  ((bool) ((msr & 0x01UL) >> 1))
#define K6_MTRR_OFFSET(msr) (msr & 0xFFFE0000UL)
#define K6_MTRR_MASK(msr)   ((msr & 0x00001FFFCUL) >> 2)

/* Returns FALSE if mask is invalid. */
static bool cpu_K6_getSizeKBFromMTRRMask(u32 mask, u32 *lengthOut) {
    size_t i;
    DBG("cpu_K6_getSizeKBFromMTRRMask: mask %08lx\n", mask);
    for (i = 0; i < MTRR_MASK_COUNT; i++) {
            if (cpu_K6_mtrrMaskTable[i].mask == mask) {
                *lengthOut = cpu_K6_mtrrMaskTable[i].sizeKB;
                return true;
            }
    }
    *lengthOut = 0UL;
    return false;
}

static void cpu_K6_decodeMTRRs(cpu_K6_MemoryTypeRangeRegs *mtrr, const sys_CPUMSR *msr) {
    mtrr->configs[0].offset         = K6_MTRR_OFFSET(msr->lo);
    mtrr->configs[0].uncacheable    = K6_MTRR_IS_UC(msr->lo);
    mtrr->configs[0].writeCombine   = K6_MTRR_IS_WC(msr->lo);
    mtrr->configs[0].isValid        = cpu_K6_getSizeKBFromMTRRMask(K6_MTRR_MASK(msr->lo), &mtrr->configs[0].sizeKB);
    mtrr->configs[0].isValid       &= (msr->lo != 0UL);

    mtrr->configs[1].offset         = K6_MTRR_OFFSET(msr->hi);
    mtrr->configs[1].uncacheable    = K6_MTRR_IS_UC(msr->hi);
    mtrr->configs[1].writeCombine   = K6_MTRR_IS_WC(msr->hi);
    mtrr->configs[1].isValid        = cpu_K6_getSizeKBFromMTRRMask(K6_MTRR_MASK(msr->hi), &mtrr->configs[1].sizeKB);
    mtrr->configs[1].isValid       &= (msr->hi != 0UL);
}

bool cpu_K6_getMemoryTypeRanges(cpu_K6_MemoryTypeRangeRegs *regs) {
    sys_CPUMSR msr;
    bool success = sys_cpuReadMSR(CPU_K6_MSR_UWCCR, &msr);
    L866_NULLCHECK(regs);

    if (!success) {
        return false;
    }

    cpu_K6_decodeMTRRs(regs, &msr);
    return true;
}

static void cpu_K6_encodeMTRRs(sys_CPUMSR *msr, const cpu_K6_MemoryTypeRangeRegs *mtrr) {
    memset(msr, 0, sizeof(sys_CPUMSR));

    if (mtrr->configs[0].isValid)
        msr->lo = (mtrr->configs[0].offset & 0xFFFE0000UL)
            | (cpu_K6_getBestMTTRMaskFromSizeKB(mtrr->configs[0].sizeKB) << 2UL)
            | (mtrr->configs[0].writeCombine ? 0x02UL : 0x00UL)
            | (mtrr->configs[0].uncacheable ? 0x01UL : 0x00UL);

    if (mtrr->configs[1].isValid)
        msr->hi = (mtrr->configs[1].offset & 0xFFFE0000UL)
            | (cpu_K6_getBestMTTRMaskFromSizeKB(mtrr->configs[1].sizeKB) << 2UL)
            | (mtrr->configs[1].writeCombine ? 0x02UL : 0x00UL)
            | (mtrr->configs[1].uncacheable ? 0x01UL : 0x00UL);

    DBG("cpu_K6_encodeMTRRs: [0x%08lx, 0x%08lx]\n", msr->lo, msr->hi);
}

bool cpu_K6_setMemoryTypeRanges(const cpu_K6_MemoryTypeRangeRegs *regs) {
    sys_CPUMSR  msr;

    L866_NULLCHECK(regs);
    cpu_K6_encodeMTRRs(&msr, regs);
    return sys_cpuWriteMSRAndVerify(CPU_K6_MSR_UWCCR, &msr);
}

bool cpu_K6_setL1Cache(bool enable) {
    bool success = true;
    u32 cr0;

    success &= sys_cpuReadControlRegister(0, &cr0);
    cr0 &= 0xBFFFFFFFUL; /* Mask Cache Disable */
    cr0 |= ((enable) ? 0UL : 0x40000000UL );
    success &= sys_cpuWriteControlRegister(0, &cr0);
    return success;
}

bool cpu_K6_setL2Cache(bool enable) {
    sys_CPUMSR  msr;
    bool        success = true;

    success &= sys_cpuReadMSR(CPU_K6_MSR_EFER, &msr);
    msr.lo &= 0xFFFFFFEFUL; /* Mask L2 Disable */
    msr.lo |= ((enable) ? 0UL : 0x00000010UL);
    success &= sys_cpuWriteMSR(CPU_K6_MSR_EFER, &msr);
    return success;
}

bool cpu_K6_getL1CacheStatus(void) {
    u32 cr0;
    L866_ASSERT(true == sys_cpuReadControlRegister(0, &cr0));
    return (cr0 & 0x40000000UL) == 0UL;
}

bool cpu_K6_getL2CacheStatus(void) {
    sys_CPUMSR msr;
    L866_ASSERT(true == sys_cpuReadMSR(CPU_K6_MSR_EFER, &msr));
    return (msr.lo & 0x00000010UL) == 0UL;
}

bool cpu_K6_setDataPrefetch(bool enable) {
    sys_CPUMSR  msr;
    bool        success = true;

    success &= sys_cpuReadMSR(CPU_K6_MSR_EFER, &msr);
    msr.lo &= 0xFFFFFFFDUL; /* Mask Data Prefetch Enable */
    msr.lo |= ((enable) ? 0x00000002UL : 0);
    success &= sys_cpuWriteMSR(CPU_K6_MSR_EFER, &msr);
    return success;
}
