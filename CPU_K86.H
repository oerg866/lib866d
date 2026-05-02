/*  LIB866D
    AMD K86 Family CPU manipulation functions
    (K5, K6)

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _CPU_K86_H_
#define _CPU_K86_H_

#include "types.h"

typedef enum {
    CPU_K86_WRITEORDER_ALL              = 0,    /* All (Slow) */
    CPU_K86_WRITEORDER_ALL_EXCEPT_UC_WC = 1,    /* All except Uncacheable/Write-Combined (Fast) */
    CPU_K86_WRITEORDER_NONE             = 2,    /* None (Fastest) */
    __CPU_K86_WRITEORDER_MODE_COUNT__
} cpu_K86_WriteOrderMode;

typedef struct {
    u32     sizeKB;
    bool    memoryHole;
} cpu_K86_WriteAllocateConfig;

typedef struct {
    bool isValid;
    u32 offset;
    u32 sizeKB;
    bool writeCombine;
    bool uncacheable;
} cpu_K86_MemoryTypeRange;

typedef struct {
    cpu_K86_MemoryTypeRange configs[2];
} cpu_K86_MemoryTypeRangeRegs;

typedef enum {
    SETMUL_OK = 0,
    SETMUL_BADMUL = 1,
    SETMUL_ERROR = 2
} cpu_K86_SetMulError;

/*  AMD K6-2 CXT/2+/III/III+: Enables/Disables Enhanced Power Management Register (EPMR) I/O block access. */
bool cpu_K86_enableEPMRIOBlock(bool enable);
/*  AMD K6-2+/III+: Set CPU Frequency Multiplier
    Example: sys_K6setMultiplier(2,5); to set a multiplier of 2.5.
    Returns:
        SETMUL_OK on success
        SETMUL_BADMUL on invalid multiplier values
        SETMUL_ERROR on other errors */
cpu_K86_SetMulError cpu_K86_setMultiplier(u16 whole, u16 fraction);
/*  AMD K6-2 CXT/2+/III/III+: Set Write Ordering Mode.
    Mode corresponds to a cpu_K86_WriteOrderMode value. */
bool cpu_K86_setWriteOrderMode(cpu_K86_WriteOrderMode mode);
/*  AMD K6-2 CXT/2+/III/III+: Enable Write Allocate for a given range configuration. */
bool cpu_K86_setWriteAllocateRange(const cpu_K86_WriteAllocateConfig *config);
/*  AMD K6-2 CXT/2+/III/III+: Enable Write Allocate for a given range configuration. */
bool cpu_K86_setWriteAllocateRangeValues(u32 sizeKB, bool memoryHole);
/*  AMD K6-2 CXT/2+/III/III+: Get current Write Allocate Configuration. */
bool cpu_K86_getWriteAllocateRange(cpu_K86_WriteAllocateConfig *config);

/*  AMD K6-2 CXT/2+/III/III+: Get memory type range registers (MTRRs). */
bool cpu_K86_getMemoryTypeRanges(cpu_K86_MemoryTypeRangeRegs *regs);
/*  AMD K6-2 CXT/2+/III/III+: Set memory type range registers (MTRRs).
    Sets both MTRRs at once. To modify a single MTRR,
    call cpu_K86_getMemoryTypeRanges first and then modify one. */
bool cpu_K86_setMemoryTypeRanges(const cpu_K86_MemoryTypeRangeRegs *regs);

/* AMD K6-2 CXT/2+/III/III+: Turns the L1 cache ON or OFF. */
bool cpu_K86_setL1Cache(bool enable);
/* AMD K6-2 2+/III/III+: Turns the L2 cache ON or OFF. */
bool cpu_K86_setL2Cache(bool enable);

/* AMD K6-2 CXT/2+/III/III+: Returns 'True' if the L1 cache is enabled. */
bool cpu_K86_getL1CacheStatus(void);
/* AMD K6-2 2+/III/III+: Returns 'True' if the L2 cache is enabled. */
bool cpu_K86_getL2CacheStatus(void);

/* AMD K6-2 CXT/2+/III/III+: Enables Data Prefetch (DPE) */
bool cpu_K86_setDataPrefetch(bool enable);

#endif
