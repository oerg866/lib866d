/*  LIB866D
    Low-Level System / BIOS / CPU Functionality

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _SYS_H_
#define _SYS_H_

#include "types.h"

#pragma warning(disable: 4001) /* Non-'int' bitfields in Microsoft C */

#pragma pack(1)

typedef struct {
    u32 lo; /* msr[31..00] = EAX from RDMSR instruction */
    u32 hi; /* msr[63..32] = EDX from RDMSR instruction */
} sys_CPUMSR;

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
} sys_CPUIDVersionInfo;

typedef struct {
    void _huge *aligned; /* The aligned buffer that is guaranteed not to cross a DMA boundary. */
    u16 alignedSize;
    void _huge *rawPtr;  /* The raw pointer used to free it later. */
    u32 rawSize;
} sys_DMABuffer;

#pragma pack()

typedef enum {
    SYS_CPU_MFR_AMD = 0,
    SYS_CPU_MFR_IDT,
    SYS_CPU_MFR_CYRIX,
    SYS_CPU_MFR_INTEL,
    SYS_CPU_MFR_TRANSMETA,
    SYS_CPU_MFR_NATSEMI,
    SYS_CPU_MFR_NEXGEN,
    SYS_CPU_MFR_RISE,
    SYS_CPU_MFR_SIS,
    SYS_CPU_MFR_UMC,
    SYS_CPU_MFR_DMP,
    SYS_CPU_MFR_ZHAOXIN,
    SYS_CPU_MFR_HYGON,
    SYS_CPU_MFR_RDC,
    SYS_CPU_MFR_MCST,
    SYS_CPU_MFR_VIA,
    SYS_CPU_MFR_AMDK5ES,
    SYS_CPU_MFR_MISTER,
    SYS_CPU_MFR_MICROSOFT,
    SYS_CPU_MFR_APPLE,
    SYS_CPU_MFR_UNKNOWN,
    ___SYS_CPU_MFR_COUNT___ = SYS_CPU_MFR_UNKNOWN,
} sys_CPUManufacturer;

typedef enum {
    OS_PURE_DOS,
    OS_WIN_REAL_MODE,
    OS_WIN_STANDARD_MODE,
    OS_WIN_ENHANCED_MODE,
    OS_WIN_95,
    OS_WIN_98,
    OS_WIN_ME,
    OS_UNKNOWN,
    ___OS_WIN_MODE_COUNT__,
} sys_osWindowsMode;

/* Generic typedef for an interrupt service routine */
typedef void (_interrupt _far * sys_ISR)(void);

/*  Attempts to figure out memory size using BIOS functions.
    hasMemoryHole is optional ('true' will be written here if a 15M memory hole is found.)
    Returns the amount of memory in bytes on success.
    Returns 0 on error. */
u32 sys_getMemorySize(bool *hasMemoryHole);

/*  Get the pyhsical 32-bit address of a FAR pointer */
u32 sys_getPhysicalAddress(void _far *ptr);

/*  Retreives the CPUID String from the CPU and places it in outStr.
    outStr must be at least 13 bytes in size (12 + 1 for null terminator).
    As of now, always returns true (until I implement detection...)
    WARNING: ONLY CALL THIS ON SUPPORTED CPUS! */
bool sys_getCPUIDString(char *outStr);
/*  Retreives the CPU Type / Family info (CPUID Level 1) data.
    WARNING: ONLY CALL THIS ON SUPPORTED CPUS! */
sys_CPUIDVersionInfo sys_getCPUIDVersionInfo(void);
/*  Get the manufacturer of the current CPU based on the CPUID.
    Returns SYS_CPU_MFR_UNKNOWN if unknown.
    'mfrClearName' is optional and will point to the manufacturer string clear name if known. */
sys_CPUManufacturer sys_getCPUManufacturer(const char **mfrClearName);

/*  Read CPU Manufacturer Specific Register (Dangerous!) */
bool sys_cpuReadMSR(u32 msrId, sys_CPUMSR *msr);
/*  Write CPU Manufacturer Specific Register (Dangerous!) */
bool sys_cpuWriteMSR(u32 msrId, const sys_CPUMSR *msr);
/*  Write CPU Manufacturer Specific Register (Dangerous!)
    and verify that the register has been written correctly. */
bool sys_cpuWriteMSRAndVerify(u32 msrId, const sys_CPUMSR *msr);

bool sys_cpuReadControlRegister(u8 index, u32 *out);
bool sys_cpuWriteControlRegister(u8 index, const u32 *in);

/*  Checks if the CPU is in Virtual 8086 mode (e.g. if EMM386 is loaded) */
bool sys_cpuIsInV86Mode();

/*  writes a 32-Bit value to <port> */
void sys_outPortL(u16 port, u32 outVal);
/*  reads a 32-Bit value from <port> */
u32 sys_inPortL(u16 port);

/*  Executes <n> IO Delay loops (akin to linux kernel udelay) */
void sys_ioDelay(u16 loops);

/*  Gets the current mode of Windows that is running. */
sys_osWindowsMode sys_getWindowsMode(void);

/*  Allocates a buffer that is guaranteed to have at least <size> bytes of space 
    and does not cross a 64K page boundary. Maximum size: 65536 */
bool sys_allocateDMABuffer(sys_DMABuffer *buf, u32 size);
/*  Frees DMA buffer.*/
void sys_freeDMABuffer(sys_DMABuffer *buf);

#endif
