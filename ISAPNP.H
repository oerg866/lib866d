
#ifndef _ISAPNP_H_
#define _ISAPNP_H_

/*  LIB866D
    ISA PnP Functions

    (C) 2026 E. Voirin (oerg866)
*/

#include <stdlib.h>

#include "types.h"

/* PnP BIOS Installation Check Structure (found in ROM) */
#pragma pack(1)
typedef struct {
    char        signature[4];       /* "$PnP" */
    u8          version;            /* BCD: 0x10 = 1.0 */
    u8          length;             /* Structure length in 16-byte units */
    u16         controlField;       /* Event notification flags */
    u8          checksum;           /* Sum of all bytes must be 0 */
    u32         eventNotifyFlag;    /* Event notification flag address */
    void _far  *rmEntry;            /* Real-mode entry point */
    u16         pmEntryOffset;      /* 16-bit PM entry point offset */
    u32         pmEntryBase;        /* 16-bit PM code segment base */
    u32         oemDevId;           /* OEM device identifier */
    u16         rmDataSegment;      /* Real-mode data segment */
    u32         pmDataBase;         /* 16-bit PM data segment base */
} pnp_BiosInfo;

typedef union {
    u32 dword;
    u8  bytes[4];
} pnp_CardId;

typedef struct {
    u16 base;   /* base bits 23 - 16 */
    union { u8 ctrl; struct {
        u8 isUpperLimit : 1;
        u8 is16Bit      : 1;
        u8 reserved     : 6;
    };};
    u16 limitRange;
    u8 padding[3];
} pnp_Mem24Cfg;

typedef struct {
    u32 base;                   /* Base Address */
    union { u8 ctrl; struct {   /* Control reg */
        u8 isUpperLimit : 1;        /* bit [0]   RO — 0=range length, 1=upper limit address  */
        u8 memWidth     : 2;        /* bits[2:1] — 00=8bit, 01=16bit, 10=reserved, 11=32bit  */
        u8 reserved     : 5;        /* bits[7:3] — must return 0                              */
    };};
    u32 limitRange;             /* Limit or Range (depends on isUpperLimit) */
    u8 paddding[1];
} pnp_Mem32Cfg;

typedef struct {
    u16 port;
} pnp_IoCfg;

typedef struct {
    u8  level;
    struct {
        u8 triggerType  : 1;    /* 1 = level, 0 = edge*/
        u8 activeHigh   : 1;
    };
} pnp_IrqCfg;

typedef struct {
    u8 ch;
} pnp_DmaCfg;

/* logical device info, up to 4 logical devs */
typedef struct {
    u8                      active;
    pnp_CardId              eisaId;
    char                    idStr[8];
    bool                    usesMem32;
    pnp_Mem24Cfg            mem24[4];
    pnp_Mem32Cfg            mem32[4];
    pnp_IoCfg               io[8];      /* up to 8 IO ranges */
    pnp_IrqCfg              irq[2];
    pnp_DmaCfg              dma[2];
} pnp_LogicalDeviceInfo;

typedef struct {
    u8                      csn;
    pnp_CardId              eisaId;
    char                    idStr[8];
    pnp_LogicalDeviceInfo   logDev[4];
    u8                      numLogDevs;
} pnp_DeviceInfo;

#pragma pack()

#define PNP_MAX_MEM_DESCRIPTORS (4)
#define PNP_MAX_IO_DESCRIPTORS  (8)
#define PNP_MAX_IRQ_DESCRIPTORS (2)
#define PNP_MAX_DMA_DESCRIPTORS (2)

/*  Detect ISA PnP BIOS. True if found, False if not.
    Info will contain the header structure if successful. */
bool pnp_biosDetect(pnp_BiosInfo *info);
/*  Get device data from all devices. MaxCards is the size of the devices array supplied. 
    Returns amount of devices the array was populated with. */
size_t pnp_getDeviceData(pnp_DeviceInfo *devices, size_t maxCards);
/*  Get device data for a single device by PNP string (e.g. ESS0968) */
bool pnp_getDeviceDataByString(pnp_DeviceInfo *dst, const char *toFind);
/*  Get pointer to logical device inside a pnpp device */
pnp_LogicalDeviceInfo *pnp_getLogicalDevice(pnp_DeviceInfo *dst, u16 index);
/*  Check if the memory region #<index> in the logical device <ld> is active. */
bool pnp_memRangeIsActive(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Gets the memory region #<index> base address in <ld> */
u32 pnp_memRangeGetBase(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Gets the memory region #<index> end address in <ld> */
u32 pnp_memRangeGetEnd(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Check if the IO port #<index> in the logical device <ld> is active. */
bool pnp_ioPortIsActive(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Gets the IO port #<index> end address in <ld> */
u16 pnp_ioPortGet(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Check if the IRQ #<index> in the logical device <ld> is active. */
bool pnp_irqIsActive(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Returns whether IRQ #<index> in <ld> is active high or low */
bool pnp_irqIsActiveHigh(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Returns whether IRQ #<index> in <ld> is level triggered or edge triggered */
bool pnp_irqIsLevelTriggered(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Gets the IO port #<index> in <ld> */
u8 pnp_irqGet(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Check if the DMA #<index> in the logical device <ld> is active. */
bool pnp_dmaIsActive(pnp_LogicalDeviceInfo *ld, u16 index);
/*  Gets the DMA channel #<index> in <ld> */
u8 pnp_dmaGet(pnp_LogicalDeviceInfo *ld, u16 index);

#endif

