/*  LIB866D
    Low-Level CPU Functionality

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef _CPU_H_
#define _CPU_H_

#include "types.h"

#ifdef _MSC_VER
#pragma warning(disable: 4001) /* Non-'int' bitfields in Microsoft C */
#endif

typedef struct {
    u32 lo; /* msr[31..00] = EAX from RDMSR instruction */
    u32 hi; /* msr[63..32] = EDX from RDMSR instruction */
} cpu_MSR;

typedef struct {
    struct {
        u16 stepping    : 4;
        u16 model       : 4;
        u16 family      : 4;
        u16 type        : 2;
        u16 __rsvd__    : 2;
    } basic;
    struct {
        u16 model       : 4;
        u16 family      : 8;
        u16 __rsvd__    : 4;
    } extended;
} cpu_CPUIDVersionInfo;

typedef enum {
    CPU_MFR_AMD = 0,
    CPU_MFR_IDT,
    CPU_MFR_CYRIX,
    CPU_MFR_INTEL,
    CPU_MFR_TRANSMETA,
    CPU_MFR_NATSEMI,
    CPU_MFR_NEXGEN,
    CPU_MFR_RISE,
    CPU_MFR_SIS,
    CPU_MFR_UMC,
    CPU_MFR_DMP,
    CPU_MFR_ZHAOXIN,
    CPU_MFR_HYGON,
    CPU_MFR_RDC,
    CPU_MFR_MCST,
    CPU_MFR_VIA,
    CPU_MFR_AMDK5ES,
    CPU_MFR_MISTER,
    CPU_MFR_MICROSOFT,
    CPU_MFR_APPLE,
    CPU_MFR_UNKNOWN,
    ___CPU_MFR_COUNT___ = CPU_MFR_UNKNOWN,
} cpu_Manufacturer;


/*  Retreives the CPUID String from the CPU and places it in outStr.
    outStr must be at least 13 bytes in size (12 + 1 for null terminator).
    As of now, always returns true (until I implement detection...)
    WARNING: ONLY CALL THIS ON SUPPORTED CPUS! */
bool cpu_getCPUIDString(char *outStr);
/*  Retreives the CPU Type / Family info (CPUID Level 1) data.
    WARNING: ONLY CALL THIS ON SUPPORTED CPUS! */
cpu_CPUIDVersionInfo cpu_getCPUIDVersionInfo(void);
/*  Get the manufacturer of the current CPU based on the CPUID.
    Returns CPU_MFR_UNKNOWN if unknown.
    'mfrClearName' is optional and will point to the manufacturer string clear name if known. */
cpu_Manufacturer cpu_getManufacturer(const char **mfrClearName);

/*  Read CPU Manufacturer Specific Register (Dangerous!) */
bool cpu_readMSR(u32 msrId, cpu_MSR *msr);
/*  Write CPU Manufacturer Specific Register (Dangerous!) */
bool cpu_writeMSR(u32 msrId, const cpu_MSR *msr);
/*  Write CPU Manufacturer Specific Register (Dangerous!)
    and verify that the register has been written correctly. */
bool cpu_writeMSRAndVerify(u32 msrId, const cpu_MSR *msr);

bool cpu_readControlRegister(u8 index, u32 *out);
bool cpu_writeControlRegister(u8 index, const u32 *in);

/*  Checks if the CPU is in Virtual 8086 mode (e.g. if EMM386 is loaded) */
bool cpu_isInV86Mode(void);

#endif
