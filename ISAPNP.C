/*  LIB866D
    ISA PnP Functions

    (C) 2026 E. Voirin (oerg866)
*/

#include "isapnp.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <dos.h>
#include <conio.h>

#include "sys.h"
#include "util.h"

#include "386ASM.H"

#define __LIB866D_TAG__ "ISAPNP"
#include "debug.h"


#pragma pack (1)

static void pnp_decodeEisaId(pnp_CardId id, char buf[8]) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = 0x40 + ((id.bytes[0] & 0x7F) >> 2);
    buf[1] = 0x40 + ((id.bytes[0] & 0x03) << 3) + (id.bytes[1] >> 5);
    buf[2] = 0x40 + (id.bytes[1] & 0x1F);
    buf[3] = hex[id.bytes[2] >> 4];
    buf[4] = hex[id.bytes[2] & 0x0F];
    buf[5] = hex[id.bytes[3] >> 4];
    buf[6] = hex[id.bytes[3] & 0x0F];
    buf[7] = 0x00;
}

bool pnp_biosDetect(pnp_BiosInfo *info) {
    /*
    * Scan BIOS ROM area F000:0000 - F000:FFF0 for the "$PnP" signature.
    * Candidates appear on 16-byte boundaries.
    * Validate with a byte-wise checksum over the header.
    */
    const char pnpSig[] = "$PnP";
    
    u32 off = 0;

    L866_NULLCHECK(info);

    memset(info, 0, sizeof(*info));

    for (off = 0x0000UL; off <= 0xFFF0UL; off += 0x10UL) {
        u8 _far *rom = (u8 _far *)MK_FP(0xF000, (u16)off); /* far ptr to F000 segment */
        pnp_BiosInfo _far *hdr = (pnp_BiosInfo _far *) rom;
        u8 checksum = 0;
        u16 i;
        u16 off16 = (u16) off;

        /* Check for "$PnP" signature */
        if (0 != _fmemcmp(rom, pnpSig, 4)) {
            continue; /* PnP Signature not found */
        }

        if (hdr->length != sizeof(pnp_BiosInfo)) {
            DBG("size mismatch, expected %02x, got %02x", sizeof(pnp_BiosInfo), hdr->length);
            continue;   /* Malformed, skip */
        }

        /* Verify checksum: all bytes in structure must sum to 0 */
        for (i = 0; i < hdr->length; i++) {
            checksum += rom[i];
        }

        DBG("$PnP found at F000:%04x, len 0x%04x, checksum %02x\n", off16, hdr->length, (u16) checksum);

        if (checksum != 0) {
            DBG("checksum FAILED\n");
            continue;
        }

        /* Valid PnP BIOS found, copy data */
        _fmemcpy((void _far *)info, hdr, sizeof(pnp_BiosInfo));

        DBG("PnP BIOS found at F000:%04X\n", (u16)off);
        DBG("  Version:        %d.%d\n", (hdr->version >> 4) & 0x0F, hdr->version & 0x0F);
        DBG("  RM Entry Point: %Fp\n", hdr->rmEntry);
        DBG("  RM Data Seg:    %04X\n", hdr->rmDataSegment);

        return true;
    }



    DBG("PnP BIOS not found.\n");
    return false;
}

#define PNP_ADDRESS   0x0279    /* write: register index (write only) */
#define PNP_WRITE     0x0A79    /* write: register data  (write only) */
#define PNP_READ      0x0213    /* read:  register data  (read  only) */

/* Configuration registers (written via ADDRESS + WRITE ports) */
#define PNP_REG_SET_READPORT    0x00   /* Set Read Data Port           */
#define PNP_REG_ISOLATION       0x01   /* Isolation register           */
#define PNP_REG_CONFIG_CTRL     0x02   /* Card Config Control          */
    #define PNP_CTRL_RESET_CSN      BIT(2) /* Reset CSN to 0        */
    #define PNP_CTRL_WAIT_KEY       BIT(1) /* Wait For Key state    */
    #define PNP_CTRL_RESET_DEV      BIT(0) /* Reset logical devices and restore registers */
#define PNP_REG_WAKE_CSN        0x03   /* Wake cards with CSN          */
#define PNP_REG_RESOURCEDATA    0x04   /* Resource data register       */
#define PNP_REG_STATUS          0x05   /* Resource read status         */
    #define PNP_STATUS_READY        BIT(0) /* Resource data is ready */
#define PNP_REG_CSN             0x06   /* Card Select Number           */
#define PNP_REG_LOGDEV          0x07   /* Logical Device Select        */
#define PNP_REG_ACTIVATE        0x30   /* 1 = activate logical dev     */
#define PNP_REG_MEM24_0         0x40   /* Mem24 base start         */
#define PNP_REG_MEM24(x)        ((u8)(PNP_REG_MEM24_0 + 8 * (x)))
#define PNP_REG_MEM32_0         0x76
#define PNP_REG_MEM32(x)        ((u8)((x) == 0 ? PNP_REG_MEM32_0 : (0x80 + 16 * (x))))
#define PNP_REG_IO0_HI          0x60   /* IO descriptor 0 base high    */
#define PNP_REG_IO0_LO          0x61   /* IO descriptor 0 base low     */
#define PNP_REG_IO(x)           ((u8)(PNP_REG_IO0_HI + (2*(x)))) /* x range 0 - 7*/
#define PNP_REG_IRQ0_NUM        0x70   /* IRQ 0 number                 */
#define PNP_REG_IRQ0_TYPE       0x71   /* IRQ 0 type                   */
#define PNP_REG_IRQ(x)          ((u8)(PNP_REG_IRQ0_NUM + (2 * (x)))) /* x range 0 - 1 */
#define PNP_REG_DMA0            0x74   /* DMA channel 0                */
#define PNP_REG_DMA(x)          ((u8)(PNP_REG_DMA0 + (x)))    /* x range 0 - 1 */


#define PNP_S_PNP_VER       0x01
#define PNP_S_LOG_DEV_ID    0x02
#define PNP_S_COMPAT_ID     0x03
#define PNP_S_IRQ           0x04
#define PNP_S_DMA           0x05
#define PNP_S_START_DEP     0x06
#define PNP_S_END_DEP       0x07
#define PNP_S_IO            0x08
#define PNP_S_IO_FIXED      0x09
#define PNP_S_END_TAG       0x0F
#define PNP_S_VENDOR        0x0E

#define PNP_L_ANSI_ID       0x02
#define PNP_L_UNICODE_ID    0x03
#define PNP_L_VENDOR        0x04
#define PNP_L_MEM32         0x05
#define PNP_L_MEM32_FIXED   0x06

static pnp_ResourceList *pnp_dependentFunctionListGrow(pnp_DependentFunctionList *dfList) {
    u16 newCount = dfList->count + 1;
    dfList->funcs = realloc(dfList->funcs, newCount * sizeof(pnp_DependentFunctionList));

    L866_NULLCHECK(dfList->funcs);

    memset(&dfList->funcs[newCount-1], 0, sizeof(pnp_DependentFunctionList));

    dfList->count = newCount;

    return &dfList->funcs[newCount-1];
}

static bool pnp_resourceListAppend(pnp_ResourceList *list, pnp_Resource *toAdd) {
    u16 newCount = list->count + 1;

    list->items = realloc(list->items, newCount * sizeof(pnp_Resource));

    L866_NULLCHECK(list->items);
    L866_NULLCHECK(toAdd);

    list->items[newCount-1] = *toAdd;

    list->count = newCount;
    return true;
}

static void pnp_freeResourceList(pnp_ResourceList *list) {
    if (list == NULL) return;

    if (list->items != NULL) {
        free(list->items);
        list->items = NULL;
    }
    list->count = 0;
}

void pnp_freeDeviceData(pnp_DeviceInfo *info) {
    size_t i;
    size_t df;
    if (info == NULL) return;

    for (i = 0; i < info->numLogDevs; i++) {
        /* Free resources */
        pnp_freeResourceList(&info->logDev[i].resources);

        /* Free all DFs (= ResourceListLists)*/
        for (df = 0; df < info->logDev[i].dfList.count; df++) {
            pnp_freeResourceList(&info->logDev[i].dfList.funcs[df]);
        }
    }
}


static u8 pnp_readReg(u8 reg) {
    u8 ret;
    outp(PNP_ADDRESS, reg);
    sys_ioDelay(10);
    ret = inp(PNP_READ);
    sys_ioDelay(10);
    return ret;
}

static void pnp_readStruct(void *buf, u8 reg, size_t size) {
    u8 *dst = (u8 *)buf;
    while (size--) {
        *dst = pnp_readReg(reg);
        dst++;
        reg++;
    }
}

static void pnp_writeReg(u8 reg, u8 val) {
    outp(PNP_ADDRESS, reg);
    sys_ioDelay(10);
    outp(PNP_WRITE, val);
    sys_ioDelay(10);
}

static void pnp_writeStruct(void *buf, u8 reg, size_t size) {
    u8 *src = (u8 *)buf;
    while (size--) {
        pnp_writeReg(reg, *src);
        src++;
        reg++;
    }
}

static bool pnp_writeStructVerify(void *buf, u8 reg, size_t size) {
    u8 *src = (u8 *)buf;
    while (size--) {
        u8 written;
        pnp_writeReg(reg, *src);
        written = pnp_readReg(reg);

        if (written != *src) {
            DBG("writeStructVerify failed: reg %02x w %02x != r %02x\n", reg, *src, written);
            return false;
        }

        src++;
        reg++;
    }

    return true;
}

static bool pnp_readResourceByte(u8 *dst) {
    /* Poll status bit until ready */
    u16 retries = 10;
    u8 status = 0;

    while (retries--) { 
        status = pnp_readReg(PNP_REG_STATUS);
        if (status & 0x01)  {
            *dst = pnp_readReg(PNP_REG_RESOURCEDATA);
            break;
        }
    };

    DBG("readResoureBytes status %02x %s %02x\n", status, (status & 1) ? "OK  " : "FAIL", *dst);
    return (status & 1) == 1;
}

static bool pnp_readResourceStructWithMaxSize(void *buf, size_t dstSize, size_t srcSize) {
    u8 *dst = (u8 *)buf;
    while (srcSize--) {
        u8 data;
        if (!pnp_readResourceByte(&data)) return false;
        if (dstSize--) {
            *dst = data;
            dst++;
        }
    }
    return true;
}

/* Step 1: Send initiation key, puts all cards into config state */
static void pnp_sendInitKey(void) {
    static const u8 initKey[] = {
        0x6A, 0xB5, 0xDA, 0xED, 0xF6, 0xFB, 0x7D, 0xBE,
        0xDF, 0x6F, 0x37, 0x1B, 0x0D, 0x86, 0xC3, 0x61,
        0xB0, 0x58, 0x2C, 0x16, 0x8B, 0x45, 0xA2, 0xD1,
        0xE8, 0x74, 0x3A, 0x9D, 0xCE, 0xE7, 0x73, 0x39,
    };

    u16 i;

    /* Write 0x00 twice to enter initiation state */
    outp(PNP_ADDRESS, 0x00);
    sys_ioDelay(10);
    outp(PNP_ADDRESS, 0x00);
    sys_ioDelay(10);

    /* Send the 32-byte LFSR key */
    for (i = 0; i < 32; i++) {
        outp(PNP_ADDRESS, initKey[i]);
        sys_ioDelay(10);
    }
}

static u8 pnp_readWithDelay() {
    u8 ret = inp(PNP_READ);
    sys_ioDelay(10);
    return ret;
}

static u8 pnp_readSerialBit() {
    /* Each bit is read as two consecutive reads:
        first read 0x55 means bit=1, 0xAA means bit=0 */
    u16 data;
    data = pnp_readWithDelay() << 8;
    sys_ioDelay(250);
    data |= pnp_readWithDelay();
    sys_ioDelay(250);
    
//    printf("%s", data == 0x55AA ? "1" : "0");
//    DBG("r1/r2 %04x\n", data);
    
    // L866_ASSERTM(data == 0x55AA || data == 0xAA55, "Unexpected Serial read value");

    return (data == 0x55AA) ? 1 : 0;
}

static void pnp_prepareEnumeration(void) {
    /* Reset all CSNs, put all cards into Isolation state */
    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY );
    pnp_sendInitKey();

    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_RESET_CSN );
    util_sleep(2);

    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY );
    pnp_sendInitKey();
    util_sleep(2);

    /* Wake all cards with CSN=0 (unassigned) */
    pnp_writeReg(PNP_REG_WAKE_CSN, 0x00);

    /* Set read data port to 0x0203 (bits [9:2] of port >> 2) */
    pnp_writeReg(PNP_REG_SET_READPORT, (PNP_READ >> 2));
    util_sleep(1);
}

/* Read 72 bit serial ID from device that is in isolation or config state. */
static u32 pnp_read72BitSerialId(void) {
    u8 ourChecksum = 0x6A;
    u8 theirChecksum = 0;
    u32 id = 0UL;
    u16 i;

    for (i = 0; i < 72; i++) {
        u8 bit = pnp_readSerialBit();

        if (i < 64) {
            /* WTF is this, found it in linux kernel, can't find it in specs...? */
            ourChecksum = ((((ourChecksum ^ (ourChecksum >> 1)) & 0x01) ^ bit) << 7) | (ourChecksum >> 1);
        } else {
            theirChecksum |= (u32) bit << (i - 64);
        }

        if (i < 32) {
            id|= (u32) bit << (i % 32);
        }
    }

    DBG("PNP ID %08lx, our checksum %02x, their checksum %02x\n", id, ourChecksum, theirChecksum);

    if (id == 0x00000000UL || id == 0xFFFFFFFFUL) return 0UL;
    if (ourChecksum != theirChecksum) return 0UL;

    return id;
}

/* Begin to enumerate currently unassigned device. Returns the Device ID if successful, 0 if not. */
static u32 pnp_startDeviceEnumeration(u8 csn) {
    u32 id = 0UL;

    /* Tell cards to begin isolation (serial ID read) */
    util_sleep(1);
    outp(PNP_ADDRESS, PNP_REG_ISOLATION);   /* set to Isolation register */
    util_sleep(1);

    id = pnp_read72BitSerialId();

    if (id == 0UL) return 0UL;

    /* Assign CSN — card stays awake in config state,
        so we can read its config immediately */
    pnp_writeReg(PNP_REG_CSN, csn);

    DBG("startDeviceEnumeration OK, ID %08lx, CSN %02x\n", id, csn);

    return id;
}

typedef enum { rp_success, rp_error, rp_openBus, rp_endOfData } pnp_ResourcePopulationStatus;

/* Populate resource / capability lists. Returns amount of logical devices that have data. */
static size_t pnp_populateResources(pnp_DeviceInfo *dev) {
    size_t itemIndex = 0;
    bool inDF = false;
    pnp_ResourceList *currentDF = NULL;
    pnp_LogicalDeviceInfo *dst;
    bool firstDevIdParsed = false;
    size_t logDevIndex = 0;

    L866_NULLCHECK(dev);

    dst = &dev->logDev[logDevIndex];
    DBG("populateResources %p logdev %p\n", dst);

    while (true) {
        pnp_Resource cur;
        bool success = true;
        
        memset(&cur, 0, sizeof(pnp_Resource));

        /* Read first byte to get the size flag */
        if (!pnp_readResourceByte(&cur.raw[0])) return 0;

        if (cur.isLarge) {
            u16 copySize;
            /* Large resource, read size */
            success &= pnp_readResourceStructWithMaxSize(&cur.large.len, sizeof(u16), sizeof(u16));
            /*  We have the size, read everything else, destination size minus 3 because of the header
                hacky, but we have to do this for strings which are variable length while we aren't... */
            copySize = MIN(sizeof(cur.large) - 3, cur.large.len);

            if (cur.large.type == 0x7F && cur.large.len == 0xFFFF) {
                DBG("Type 0xFF and length 0xFFFF -> No resources on this log dev?\n");
                return logDevIndex;
            }

            if (!pnp_readResourceStructWithMaxSize(cur.large.data, copySize, cur.large.len)) {
                DBG("Large resource read error\n");
                return logDevIndex;
            }

            DBG("Resource %02u: Large Type %u (%u bytes, %u bytes stored)\n", itemIndex, cur.large.type, cur.large.len, copySize);
        } else {
            /* Small resource, size is already known */
            if (!pnp_readResourceStructWithMaxSize(cur.small.data, cur.small.len, cur.small.len)) {
                DBG("Small resource read error\n");
                return logDevIndex;
            }
            DBG("Resource %02u: Small Type %u (%u bytes)\n", itemIndex, cur.small.type, cur.small.len);
        }

        if (!cur.isLarge && cur.small.type == PNP_S_LOG_DEV_ID) {
            /*  If we have a second logical device id in the first logical device, assume this is the next logical device. 
                firstDevIdParsed stays true after this point, because this logic is no longer needed. */
            if (firstDevIdParsed) {
                if (inDF) {
                    DBG("WARNING: new log device inside dependent function!\n");
                    currentDF = NULL;
                    inDF = false;
                }
                /* Next logical device, code below will add the ID resource to it already */
                DBG("+++++ Logical device %u END\n", logDevIndex);
                logDevIndex++;
                L866_ASSERTM(logDevIndex < 4, "Too many logical devices");
                dst = &dev->logDev[logDevIndex];
                DBG("+++++ Logical device %u START\n", logDevIndex);
            } else {
                firstDevIdParsed = true;
                DBG("+++++ Logical device %u START\n", logDevIndex);
            }
        }

        /* Check for end tag */
        if (!cur.isLarge && cur.small.type == PNP_S_END_TAG) {
            DBG("< END OF RESOURCE PARSING: %u resources parsed >\n", itemIndex);
            return logDevIndex + 1;
        }

        /* Handle Dependent Function Start/end */
        if (!cur.isLarge && cur.small.type == PNP_S_START_DEP) {
            inDF = true;
            currentDF = pnp_dependentFunctionListGrow(&dst->dfList);
            DBG(">>> Dependency Func %u START\n", dst->dfList.count);
            continue;
        }

        if (!cur.isLarge && cur.small.type == PNP_S_END_DEP) {
            L866_ASSERTM(inDF, "DF End Tag without DF Start");
            DBG(">>> Dependency Func %u END\n", dst->dfList.count);
            currentDF = NULL;
            inDF = false;
            continue;
        }

        /* In Dependency Func? Add this resource there */
        if (inDF) {
            L866_NULLCHECK(currentDF);
            if (!pnp_resourceListAppend(currentDF, &cur)) return logDevIndex;
        } else {
            if (!pnp_resourceListAppend(&dst->resources, &cur)) return logDevIndex;
        }

        itemIndex++;
    }

    return logDevIndex;
}

/* Select logical device of currently configuring device and verifies it */
static bool pnp_switchLogicalDevice(size_t index) {
    u8 value = (u8) index;

    DBG("switchLogicalDevice %u\n", index);

    if (index >= 4) return false;

    /* switch to this logical device number */
    return pnp_writeStructVerify(&value, PNP_REG_LOGDEV, 1);
}

static void pnp_readMem32WithByteswap(pnp_Mem32Cfg *dst, size_t idx) {
    pnp_readStruct(dst, PNP_REG_MEM32(idx), sizeof(pnp_Mem32Cfg));
    util_swapInPlace32(&dst->base);
    util_swapInPlace32(&dst->limitRange);
}

static void pnp_readMem24WithByteswap(pnp_Mem24Cfg *dst, size_t idx) {
    pnp_readStruct(dst, PNP_REG_MEM24(idx), sizeof(pnp_Mem24Cfg));
    util_swapInPlace16(&dst->base);
    util_swapInPlace16(&dst->limitRange);
}

static void pnp_readIoWithByteswap(pnp_IoCfg *dst, size_t idx) {
    pnp_readStruct(dst, PNP_REG_IO(idx), sizeof(pnp_IoCfg));
    util_swapInPlace16(&dst->port);
}

static void pnp_readIrq(pnp_IrqCfg *dst, size_t idx) {
    pnp_readStruct(dst, PNP_REG_IRQ(idx), sizeof(pnp_IrqCfg));
}

static void pnp_readDma(pnp_DmaCfg *dst, size_t idx) {
    pnp_readStruct(dst, PNP_REG_DMA(idx), sizeof(pnp_DmaCfg));
}

static bool pnp_writeIoWithByteswap(size_t idx, pnp_IoCfg *toWrite) {
    pnp_IoCfg tmp = *toWrite;
    util_swapInPlace16(&tmp.port); /* MSB First */
    return pnp_writeStructVerify(&tmp, PNP_REG_IO(idx), sizeof(tmp));
}

static bool pnp_writeIrq(size_t idx, pnp_IrqCfg *toWrite) {
    return pnp_writeStructVerify(toWrite, PNP_REG_IRQ(idx), sizeof(*toWrite));
}

static bool pnp_writeDma(size_t idx, pnp_DmaCfg *toWrite) {
    return pnp_writeStructVerify(toWrite, PNP_REG_DMA(idx), sizeof(*toWrite));
}

static void pnp_logDevPopulateData(pnp_LogicalDeviceInfo *dst) {
    u16 j;
    bool is24 = false;
    bool is32 = false;

    dst->active = pnp_readReg(PNP_REG_ACTIVATE);

    for (j = 0; j < 4; j++) { pnp_readMem32WithByteswap(&dst->mem32[j], j); }
    for (j = 0; j < 4; j++) { pnp_readMem24WithByteswap(&dst->mem24[j], j); }
    for (j = 0; j < 8; j++) { pnp_readIoWithByteswap(&dst->io[j], j); }
    for (j = 0; j < 2; j++) { pnp_readIrq(&dst->irq[j], j); }
    for (j = 0; j < 2; j++) { pnp_readDma(&dst->dma[j], j); }

    is32 |= dst->mem32[0].base != 0UL;
    is24 |= dst->mem24[0].base != 0;

    L866_ASSERTM(!(is24 && is32), "Logical device appars to be using 32 AND 24 bit mem descriptors");
    dst->usesMem32 = is32;
}

static void pnp_populateDeviceInfo(pnp_DeviceInfo *device, u8 csn, u32 id) {
    pnp_DeviceInfo curdev;
    size_t logDevs;
    size_t i;

    DBG("populateDeviceInfo %p, csn %u, id %lx\n", device, csn, id);

    memset(&curdev, 0, sizeof(curdev));

    curdev.csn           = csn;
    curdev.eisaId.dword  = id;
    curdev.numLogDevs = 0;
    
    pnp_decodeEisaId(curdev.eisaId, curdev.idStr);

    DBG("Device is: %08lx %s\n", SWAP32(curdev.eisaId.dword), curdev.idStr);

    /*  First we read all the resource data. This is not a guaranteed indicator of logical device count.
        Separate loop because we cannot activate the LDN here. */
    curdev.numLogDevs = pnp_populateResources(&curdev);
    
    for (i = 0; i < curdev.numLogDevs; i++) {
        pnp_LogicalDeviceInfo *curLogDev = &curdev.logDev[i];

        if (!pnp_switchLogicalDevice(i)) {
            DBG("Error switching to logical device %u\n", i);
            break;
        }

        pnp_logDevPopulateData(curLogDev);
    }

    DBG("%u logical devices were parsed\n", curdev.numLogDevs);
    *device = curdev;
}

size_t pnp_getDeviceData(pnp_DeviceInfo *devices, size_t maxCards) {
    size_t      numCards = 0;
    u8          csn = 1;

    L866_NULLCHECK(devices);

    pnp_prepareEnumeration();
    
    while (numCards < maxCards && csn < 255) {
        u32 id = pnp_startDeviceEnumeration(csn++);

        if (id == 0UL) break;

        pnp_populateDeviceInfo(&devices[numCards++], (u8) (csn - 1), id);
        
        pnp_writeReg(PNP_REG_WAKE_CSN, 0x00);  /* wake remaining unassigned */
    }

    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY);
    return numCards;
}

bool pnp_getDeviceDataByString(pnp_DeviceInfo *dst, const char *toFind) {
    u8 csn = 1;
    bool found = false;

    L866_NULLCHECK(dst);
    L866_NULLCHECK(toFind);

    pnp_prepareEnumeration();

    while (csn < 255) {
        char toCompare[8];
        pnp_CardId id;
        id.dword = pnp_startDeviceEnumeration(csn++);

        if (id.dword == 0UL) break;

        pnp_decodeEisaId(id, toCompare);

        if (util_stringEquals(toCompare, toFind)) {
            pnp_populateDeviceInfo(dst, (u8) (csn - 1), id.dword);
            found = true;
            break;
        }

        pnp_writeReg(PNP_REG_WAKE_CSN, 0x00);  /* wake remaining unassigned */
    }

    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY);
    return found;
}

static bool pnp_activateDeviceAndSetLogicalDevice(u8 csn, size_t logDev) {
    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY);
    pnp_sendInitKey();
    util_sleep(2);

    /* Wake up our card */
    pnp_writeReg(PNP_REG_WAKE_CSN, csn);

    DBG("Wake csn %u\n", csn);

    return pnp_switchLogicalDevice(logDev);
}

bool pnp_updateDeviceData(pnp_DeviceInfo *device) {
    size_t i;

    L866_NULLCHECK(device);
    L866_ASSERT(device->csn != 0);

    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY );
    pnp_sendInitKey();
    util_sleep(2);

    /* Wake up our card */
    pnp_writeReg(PNP_REG_WAKE_CSN, device->csn);

    for (i = 0; i < device->numLogDevs; i++) {
        pnp_LogicalDeviceInfo *curLogDev = &device->logDev[i];

        if (!pnp_switchLogicalDevice(i)) {
            DBG("Error switching to logical device %u\n", i);
            break;
        }

        pnp_logDevPopulateData(curLogDev);
    }

    return true;
}

pnp_LogicalDeviceInfo *pnp_getLogicalDevice(pnp_DeviceInfo *dst, u16 index) {
    L866_NULLCHECK(dst);

    if (index >= dst->numLogDevs) return NULL;

    return &dst->logDev[index];
}

bool pnp_memRangeIsActive(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    if (ld->usesMem32) {
        return ld->mem32[index].base != 0UL;
    } else {
        return ld->mem24[index].base != 0;
    }
}

u32 pnp_memRangeGetBase(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    if (ld->usesMem32) {
        return ld->mem32[index].base;
    } else {
        return ((u32) ld->mem24[index].base) << 8;
    }
}

u32 pnp_memRangeGetEnd(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    if (ld->usesMem32) {
        if (ld->mem32[index].isUpperLimit) {
            return ld->mem32[index].limitRange;
        } else {
            return ld->mem32[index].base + ld->mem32[index].limitRange;
        }
    } else {
        if (ld->mem24[index].isUpperLimit) {
            return ((u32) ld->mem24[index].limitRange) << 8;
        } else {
            return ((u32) ld->mem24[index].base + (u32) ld->mem24[index].limitRange) << 8;
        }
    }
}

bool pnp_ioPortIsActive(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return (0 != ld->io[index].port);
}

u16 pnp_ioPortGet(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return ld->io[index].port;
}

bool pnp_irqIsActive(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return (0 != ld->irq[index].level);
}

bool pnp_irqIsActiveHigh(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return ld->irq[index].activeHigh;
}

bool pnp_irqIsLevelTriggered(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return ld->irq[index].triggerType;
}

u8 pnp_irqGet(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return ld->irq[index].level;
}

bool pnp_dmaIsActive(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return (4 != ld->dma[index].ch);
}

u8 pnp_dmaGet(pnp_LogicalDeviceInfo *ld, u16 index) {
    L866_NULLCHECK(ld);
    return ld->dma[index].ch;
}

bool pnp_getCurrentValueByTypeAndIndex(pnp_LogicalDeviceInfo *ld, pnp_SupportedValueType type, u16 index, u16 *value) {
    switch (type) {
        case pnp_svpIORange: /* fallthrough */
        //case pnp_svpIO:
            if (index >= PNP_MAX_IO_DESCRIPTORS) return false;
            *value = pnp_ioPortGet(ld, index);
            return true;
        case pnp_svpIRQ:
            if (index >= PNP_MAX_IRQ_DESCRIPTORS) return false;
            *value = pnp_irqGet(ld, index);
            return true;
        case pnp_svpDMA:
            if (index >= PNP_MAX_DMA_DESCRIPTORS) return false;
            *value = (u16) pnp_dmaGet(ld, index);
            return true;
        default:
            return false;
    }
}

bool pnp_setCurrentValueByTypeAndIndex(pnp_DeviceInfo *dev, size_t logDev, pnp_SupportedValueType type, u16 index, u16 value) {
    pnp_IoCfg io;
    pnp_DmaCfg dma;
    pnp_IrqCfg irq;
    int i;
    L866_NULLCHECK(dev);
    if (logDev >= dev->numLogDevs) return false;
    if (!pnp_activateDeviceAndSetLogicalDevice(dev->csn, logDev)) return false;

    /* Logical device is activated */

#if 0
    for (i = 0; i < 256; i++) {
        u8 a = pnp_readReg(i);
        util_sleep(1UL);
        printf("%02x ", a);
        if (i % 16 == 15) printf("\n");
    }

    getchar();
#endif
    switch (type) {
        case pnp_svpIORange: /* fallthrough */
        //case pnp_svpIO:
            if (index >= PNP_MAX_IO_DESCRIPTORS) return false;
            pnp_readIoWithByteswap(&io, index);
            io.port = value;
            return pnp_writeIoWithByteswap(index, &io);
        case pnp_svpIRQ:
            if (index >= PNP_MAX_IRQ_DESCRIPTORS) return false;
            if (value > 15) return false;
            pnp_readIrq(&irq, index);
            irq.level = value;
            return pnp_writeIrq(index, &irq);
        case pnp_svpDMA:
            if (index >= PNP_MAX_DMA_DESCRIPTORS) return false;
            if (value > 7) return false;
            pnp_readDma(&dma, index);
            dma.ch = value;
            return pnp_writeDma(index, &dma);
        default:
            return false;
    }
}

bool pnp_setLogicalDeviceActive(pnp_DeviceInfo *dev, size_t logDev, bool active) {
    u8 value = active ? 1 : 0;

    L866_NULLCHECK(dev);
    if (logDev >= dev->numLogDevs) return false;
    if (!pnp_activateDeviceAndSetLogicalDevice(dev->csn, logDev)) return false;
    return pnp_writeStructVerify(&value, PNP_REG_ACTIVATE, 1);
}

static pnp_Resource *pnp_getResourceByIndex(pnp_ResourceList *rl, size_t index) {
    return &rl->items[index];
}

static size_t pnp_getResourceCountByTag(pnp_ResourceList *rl, bool isLarge, u8 type) {
    size_t i;
    size_t matches = 0;
    L866_NULLCHECK(rl);
    for (i = 0; i < rl->count; i++) {
        pnp_Resource *cur = pnp_getResourceByIndex(rl, i);

        if (isLarge && cur->isLarge && cur->large.type == type) matches++;
        if (!isLarge && !cur->isLarge && cur->small.type == type) matches++;
    }
    return matches;
}

static pnp_Resource *pnp_getResourceByTag(pnp_ResourceList *rl, bool isLarge, u8 type, size_t index, size_t *totalCount) {
    size_t i;
    size_t leftBeforeRet = index;
    L866_NULLCHECK(rl);

    if (totalCount != NULL) *totalCount = pnp_getResourceCountByTag(rl, isLarge, type);

    DBG("getResourceByTag: Resource list count: %u\n", rl->count);

    for (i = 0; i < rl->count; i++) {
        pnp_Resource *cur = pnp_getResourceByIndex(rl, i);

        if (isLarge && cur->isLarge && cur->large.type == type)  {
            if (leftBeforeRet == 0) return cur;
            leftBeforeRet--;
        }

        if (!isLarge && !cur->isLarge && cur->small.type == type) {
            if (leftBeforeRet == 0) return cur;
            leftBeforeRet--;
        }

    }

    DBG("Resource type %u index %u not found\n", type, index);
    return NULL;
}

pnp_Resource *pnp_resIrq(pnp_ResourceList *rl, size_t index, size_t *totalCount) {
    pnp_Resource *ret = pnp_getResourceByTag(rl, false, PNP_S_IRQ, index, totalCount);
    return ret;
}

pnp_Resource *pnp_resIoFixed(pnp_ResourceList *rl, size_t index, size_t *totalCount) {
    pnp_Resource *ret = pnp_getResourceByTag(rl, false, PNP_S_IO_FIXED, index, totalCount);
    return ret;
}

pnp_Resource *pnp_resIoRange(pnp_ResourceList *rl, size_t index, size_t *totalCount) {
    pnp_Resource *ret = pnp_getResourceByTag(rl, false, PNP_S_IO, index, totalCount);
    return ret;
}

pnp_Resource *pnp_resDma(pnp_ResourceList *rl, size_t index, size_t *totalCount) {
    pnp_Resource *ret = pnp_getResourceByTag(rl, false, PNP_S_DMA, index, totalCount);
    return ret;
}

bool pnp_resString(pnp_ResourceList *rl, char *buf) {
    pnp_Resource *strRes = pnp_getResourceByTag(rl, true, PNP_L_ANSI_ID, 0, NULL);
    size_t len = 0;

    L866_NULLCHECK(buf);
    L866_NULLCHECK(rl);

    if (strRes == NULL) return false;

    len = MIN(PNP_MAX_STRING_LENGTH, strRes->large.len);
    memcpy(buf, strRes->large.str.data, len);
    buf[len] = 0x00;
    return true;
}

bool pnp_getSupportedIRQsFromDFs(DynU16 *dst, pnp_LogicalDeviceInfo *ld, size_t index) {
    size_t dfIdx;
    L866_NULLCHECK(dst);
    dst->count = 0;
    /*  Get all possible values for all Dependency Functions (== config/capability variants)
        But only the <index>'th resource of this type */
    for (dfIdx = 0; dfIdx < ld->dfList.count; dfIdx++) {
        pnp_Resource *res = pnp_resIrq(&ld->dfList.funcs[dfIdx], index, NULL);
        u16 i;
        if (res == NULL) continue;
        for (i = 0; i < 16; i++) {
            if (res->small.irq.mask & BIT(i)) {
                if (!util_dynU16Add(dst, i)) return false;
            }
        }
    }

    util_dynU16Sort(dst);
    util_dynU16Deduplicate(dst);
    return true;
}

bool pnp_getSupportedIORangeBasesFromDFs(DynU16 *dst, pnp_LogicalDeviceInfo *ld, size_t index) {
    size_t dfIdx;
    L866_NULLCHECK(dst);
    dst->count = 0;
    /*  Get all possible values for all Dependency Functions (== config/capability variants)
        But only the <index>'th resource of this type */
    for (dfIdx = 0; dfIdx < ld->dfList.count; dfIdx++) {
        pnp_Resource *res = pnp_resIoRange(&ld->dfList.funcs[dfIdx], index, NULL);
        u16 base = res->small.ioRange.baseMin;
        if (res == NULL) continue;
        DBG("DependentFunc %u processing range %04x-%04x align %x\n", dfIdx, res->small.ioRange.baseMin, res->small.ioRange.baseMax, res->small.ioRange.align);

        while (base <= res->small.ioRange.baseMax) {
            DBG("DF %u Range valid base %04x\n", dfIdx, base);
            if (!util_dynU16Add(dst, base)) {
                DBG("Failed to add, count = %u\n", dst->count);
                return false;
            }
            base += res->small.ioRange.align;
        }
    }
    DBG("before sort: %u\n", dst->count);
    util_dynU16Sort(dst);
    DBG("before dedupe: %u\n", dst->count);
    util_dynU16Deduplicate(dst);
    DBG("finished: %u\n", dst->count);
    return true;
}

bool pnp_getSupportedDMAsFromDFs(DynU16 *dst, pnp_LogicalDeviceInfo *ld, size_t index) {
    size_t dfIdx;
    L866_NULLCHECK(dst);
    dst->count = 0;
    /*  Get all possible values for all Dependency Functions (== config/capability variants)
        But only the <index>'th resource of this type */
    for (dfIdx = 0; dfIdx < ld->dfList.count; dfIdx++) {
        pnp_Resource *res = pnp_resDma(&ld->dfList.funcs[dfIdx], index, NULL);
        u16 i;
        if (res == NULL) continue;
        for (i = 0; i < 8; i++) {
            if (res->small.dma.mask & BIT(i)) {
                if (!util_dynU16Add(dst, i)) return false;
            }
        }
    }
    util_dynU16Sort(dst);
    util_dynU16Deduplicate(dst);
    return true;
}

bool pnp_getSupportedResourceValuesFromDFsByType(DynU16 *dst, pnp_LogicalDeviceInfo *ld, pnp_SupportedValueType type, size_t index) {
    L866_NULLCHECK(dst);
    L866_NULLCHECK(ld);
    
    DBG("getSupportedResourceValuesFromDFsByType type %u index %u\n", (u16) type, (u16) index);
    
    switch (type) {
        case pnp_svpIORange:    return pnp_getSupportedIORangeBasesFromDFs(dst, ld, index);
        case pnp_svpIRQ:        return pnp_getSupportedIRQsFromDFs(dst, ld, index);
        case pnp_svpDMA:        return pnp_getSupportedDMAsFromDFs(dst, ld, index);
        default:                return false;
    }
    return false;
}
