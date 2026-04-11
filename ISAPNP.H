
#ifndef _ISAPNP_H_
#define _ISAPNP_H_

/*  LIB866D
    ISA PnP Functions

    (C) 2026 E. Voirin (oerg866)
*/

#include <stdlib.h>

#include "types.h"

#include "util.h"

#define PNP_MAX_MEM_DESCRIPTORS (4)
#define PNP_MAX_IO_DESCRIPTORS  (8)
#define PNP_MAX_IRQ_DESCRIPTORS (2)
#define PNP_MAX_DMA_DESCRIPTORS (2)
#define PNP_MAX_STRING_LENGTH   (128)

#pragma pack(1)
/* PnP BIOS Installation Check Structure (found in ROM) */
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

/*********************************************************
 * Resource Data / Capability Descriptors
 * (NOT identical to the info descriptors above)
 *********************************************************/

/* IRQ (small, type 0x04, len 2 or 3) */
typedef struct {
    u16 mask;
    struct {
        u8 activeHighEdge   : 1;
        u8 activeLowEdge    : 1;
        u8 activeHighLevel  : 1;
        u8 activeLowLevel   : 1;
        u8 _rsv             : 4;
    };
} res_Irq;

/* DMA (small, type 0x05, len 2) */
typedef struct {
    u8  mask;
    struct {
        u8 width            : 2; /* 00 8bit, 01 both,  10 16bit */
        u8 isBusMaster      : 1;
        u8 allowCountByByte : 1;
        u8 allowCountByWord : 1;
        u8 dmaSpeed         : 2;
        u8 _rsv             : 1;
    };
} res_Dma;

/* I/O port range (small, type 0x08, len 7) */
typedef struct {
    struct {
        u8 decodeFull16Bit  : 1;
        u8 _rsv1            : 7;
    };
    u16 baseMin;
    u16 baseMax;
    u8  align;
    u8  len;
} res_IoRange;

/* Fixed I/O port (small, type 0x09, len 3) */
typedef struct {
    u16 base;
    u8  len;
} res_IoFixed;

/* Start Dependent Functions (small, type 0x06, len 0 or 1) */
typedef struct {
    u8  priority;
} res_StartDep;

/* End Tag (small, type 0x0F, len 1) */
typedef struct {
    u8  checksum;
} res_EndTag;

/* Memory range (Large, type 0x01, len 9) */
typedef struct {
    struct {
        u8 writeable        : 1;
        u8 cacheable        : 1;
        u8 isHighAddress    : 1; 
        u8 width            : 2; /* 00 8bit, 01 16bit, 10 both */
        u8 shadowable       : 1;
        u8 isRom            : 1;
        u8 _rsv;
    };
    u16 minBase;
    u16 maxBase;
    u16 align;
    u16 len;
} res_Mem24;

/* 32-bit memory range (large, type 0x05, len 17) */
typedef struct {
    struct {
        u8 writeable        : 1;
        u8 cacheable        : 1;
        u8 isHighAddress    : 1; 
        u8 width            : 2; /* 00 8bit, 01 16bit, 10 both */
        u8 shadowable       : 1;
        u8 isRom            : 1;
        u8 _rsv;
    };
    u32 minBase;
    u32 maxBase;
    u32 align;
    u32 len;
} res_Mem32;

/* 32-bit Fixed memory range (large, type 0x06, len 9) */
typedef struct {
    struct {
        u8 writeable        : 1;
        u8 cacheable        : 1;
        u8 isHighAddress    : 1; 
        u8 width            : 2; /* 00 8bit, 01 16bit, 10 both */
        u8 shadowable       : 1;
        u8 isRom            : 1;
        u8 _rsv;
    };
    u32 base;
    u32 len;
} res_Mem32Fixed;

typedef struct {
    char data[PNP_MAX_STRING_LENGTH];
} res_AnsiString;

/* Wire-format wrapper for one resource item */
typedef union {
    /* Raw bytes for easier header reading */
    u8 raw[3];
    /* Resource size flag */
    struct {
        u8 _rsv : 7;
        u8 isLarge : 1;
    };
    /* Small resource */
    struct {
        struct {
            u8 len  : 3;
            u8 type : 4;
            u8 _rsv : 1;
        };
        union {
            u8              data[1];
            res_Irq         irq;
            res_Dma         dma;
            res_IoRange     ioRange;
            res_IoFixed     ioFixed;
        };
    } small;
    /* Large resource */
    struct {
        struct {
            u8 type : 7;
            u8 _rsv : 1;
        };
        u16 len;
        union {
            u8              data[1];
            res_Mem24       mem24;
            res_Mem32       mem32;
            res_Mem32Fixed  mem32fixed;
            res_AnsiString  str;
        };
    } large;
} pnp_Resource;

typedef struct {
    size_t                  count;
    pnp_Resource           *items;
} pnp_ResourceList;

typedef struct {
    size_t                  count;
    pnp_ResourceList       *funcs;
} pnp_DependentFunctionList;

/* logical device info, up to 4 logical devs */
typedef struct {
    u8                          active;
    pnp_CardId                  eisaId;
    char                        idStr[8];
    bool                        usesMem32;
    pnp_Mem24Cfg                mem24[4];
    pnp_Mem32Cfg                mem32[4];
    pnp_IoCfg                   io[8];      /* up to 8 IO ranges */
    pnp_IrqCfg                  irq[2];
    pnp_DmaCfg                  dma[2];
    pnp_ResourceList            resources;
    pnp_DependentFunctionList   dfList;
} pnp_LogicalDeviceInfo;

typedef struct {
    u8                      csn;
    pnp_CardId              eisaId;
    char                    idStr[8];
    pnp_LogicalDeviceInfo   logDev[4];
    u8                      numLogDevs;
} pnp_DeviceInfo;

typedef enum {
    pnp_svpIORange = 0,     /* IO port range resource / setting */
    pnp_svpIRQ,             /* IRQ resource / setting */
    pnp_svpDMA,             /* DMA resource / setting */
} pnp_SupportedValueType;
#pragma pack()

/*  Detect ISA PnP BIOS. True if found, False if not.
    Info will contain the header structure if successful. */
bool pnp_biosDetect(pnp_BiosInfo *info);
/*  Get device data from all devices. MaxCards is the size of the devices array supplied. 
    Returns amount of devices the array was populated with.
    NOTE: Allocates resource descriptors for each logical device,
    so you MUST call freeDeviceData after usage! */
size_t pnp_getDeviceData(pnp_DeviceInfo *devices, size_t maxCards);
/*  Get device data for a single device by PNP string (e.g. ESS0968) */
bool pnp_getDeviceDataByString(pnp_DeviceInfo *dst, const char *toFind);
/*  Frees the dynamic memory used for the resource descriptors. */
void pnp_freeDeviceData(pnp_DeviceInfo *info);
/*  Update the device data for a previously configured device */
bool pnp_updateDeviceData(pnp_DeviceInfo *device);
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
/*  Obtain <type> base value #<index> in <ld>, will end up in <value>. Returns false if unsuccessful for whatever reason. */
bool pnp_getCurrentValueByTypeAndIndex(pnp_LogicalDeviceInfo *ld, pnp_SupportedValueType type, u16 index, u16 *value);
/*  Set <type> base value #<index> in <dev>'s <logdev>'th logical device,
    returns false if unsuccessful for whatever reason. Verifies that the value is read back as written too. */
bool pnp_setCurrentValueByTypeAndIndex(pnp_DeviceInfo *dev, size_t logDev, pnp_SupportedValueType type, u16 index, u16 value);
/*  Enable or disable the <logDev>'th logical device in <dev>. */
bool pnp_setLogicalDeviceActive(pnp_DeviceInfo *dev, size_t logDev, bool active);
/*  Gets the <index>th IRQ resource of resource list rl (from logical device), NULL if not found.
    If totalCount is non-NULL, it will receive the total amount of resources of this type */
pnp_Resource *pnp_resIrq(pnp_ResourceList *rl, size_t index, size_t *totalCount);
/* Gets the <index>th Fixed IO resource of resource list rl (from logical device), NULL if not found.
    If totalCount is non-NULL, it will receive the total amount of resources of this type */
pnp_Resource *pnp_resIoFixed(pnp_ResourceList *rl, size_t index, size_t *totalCount);
/* Gets the <index>th IO Range resource of resource list rl (from logical device), NULL if not found.
    If totalCount is non-NULL, it will receive the total amount of resources of this type */
pnp_Resource *pnp_resIoRange(pnp_ResourceList *rl, size_t index, size_t *totalCount);
/* Gets the <index>th DMA resource of resource list rl (from logical device), NULL if not found.
    If totalCount is non-NULL, it will receive the total amount of resources of this type */
pnp_Resource *pnp_resDma(pnp_ResourceList *rl, size_t index, size_t *totalCount);
/*  Gets the ANSI string of resource list rl (from logical device), false if not found.
    buf must be >= PNP_MAX_STRING_LENGTH + 1 in size. */
bool pnp_resString(pnp_ResourceList *rl, char *buf);

/*  Fills a dynamic u16 array with all possible values for the IRQ[index] resouce calculated from
    ALL DEPENDENT FUNCTIONS of the given logical device. */
bool pnp_getSupportedIRQsFromDFs(DynU16 *dst, pnp_LogicalDeviceInfo *ld, size_t index);
/*  Fills a dynamic u16 array with all possible values for the IORange[index] resouce calculated from
    ALL DEPENDENT FUNCTIONS of the given logical device. */
bool pnp_getSupportedIORangeBasesFromDFs(DynU16 *dst, pnp_LogicalDeviceInfo *ld, size_t index);
/*  Fills a dynamic u16 array with all possible values for the DMA[index] resouce calculated from
    ALL DEPENDENT FUNCTIONS of the given logical device. */
bool pnp_getSupportedDMAsFromDFs(DynU16 *dst, pnp_LogicalDeviceInfo *ld, size_t index);
/*  Fills a dynamic u16 array with all possible values for the <type>[index] resouce calculated from
    ALL DEPENDENT FUNCTIONS of the given logical device.
    Supports the types above and calls those functions depending on that */
bool pnp_getSupportedResourceValuesFromDFsByType(DynU16 *dst, pnp_LogicalDeviceInfo *ld, pnp_SupportedValueType type, size_t index);

#endif

