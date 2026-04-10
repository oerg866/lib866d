/*  LIB866D
    Low-Level System / BIOS / Memory Functionality

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _SYS_H_
#define _SYS_H_

#include "types.h"

#pragma warning(disable: 4001) /* Non-'int' bitfields in Microsoft C */

#pragma pack(1)

typedef struct {
    void _huge *aligned; /* The aligned buffer that is guaranteed not to cross a DMA boundary. */
    u16 alignedSize;
    void _huge *rawPtr;  /* The raw pointer used to free it later. */
    u32 rawSize;
} sys_DMABuffer;

#pragma pack()

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
typedef void (cdecl _interrupt _far * cdecl sys_ISR)();

/*  Attempts to figure out memory size using BIOS functions.
    hasMemoryHole is optional ('true' will be written here if a 15M memory hole is found.)
    Returns the amount of memory in bytes on success.
    Returns 0 on error. */
u32 sys_getMemorySize(bool *hasMemoryHole);

/*  Get the pyhsical 32-bit address of a FAR pointer */
u32 sys_getPhysicalAddress(void _far *ptr);

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
/*  Checks if drive with given letter is remote (i.e. network, SUBST, CD, etc.) */
bool sys_driveIsRemote(char letter);

#endif
