#include "isapnp.h"

/*  LIB866D
    ISA PnP Functions

    (C) 2026 E. Voirin (oerg866)
*/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <i86.h>
#include <dos.h>
#include <conio.h>

#include "sys.h"
#include "util.h"

#include "386ASM.H"

#define __LIB866D_TAG__ "ISAPNP"
#include "debug.h"

/* This code is hard to write... Maybe some other time :\ */

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
#define PNP_REG_MEM24(x)        (PNP_REG_MEM24_0 + 8 * (x))
#define PNP_REG_MEM32_0         0x76
#define PNP_REG_MEM32(x)        ((x) == 0 ? PNP_REG_MEM32_0 : (0x80 + 16 * (x)))
#define PNP_REG_IO0_HI          0x60   /* IO descriptor 0 base high    */
#define PNP_REG_IO0_LO          0x61   /* IO descriptor 0 base low     */
#define PNP_REG_IO(x)           (PNP_REG_IO0_HI + (2*(x))) /* x range 0 - 7*/
#define PNP_REG_IRQ0_NUM        0x70   /* IRQ 0 number                 */
#define PNP_REG_IRQ0_TYPE       0x71   /* IRQ 0 type                   */
#define PNP_REG_IRQ(x)          (PNP_REG_IRQ0_NUM + (2 * (x))) /* x range 0 - 1 */
#define PNP_REG_DMA0            0x74   /* DMA channel 0                */
#define PNP_REG_DMA(x)          (PNP_REG_DMA0 + (x))    /* x range 0 - 1 */

static const u8 pnp_initKey[] = {
    0x6A, 0xB5, 0xDA, 0xED, 0xF6, 0xFB, 0x7D, 0xBE,
    0xDF, 0x6F, 0x37, 0x1B, 0x0D, 0x86, 0xC3, 0x61,
    0xB0, 0x58, 0x2C, 0x16, 0x8B, 0x45, 0xA2, 0xD1,
    0xE8, 0x74, 0x3A, 0x9D, 0xCE, 0xE7, 0x73, 0x39,
};



static void pnp_writeReg(u8 reg, u8 val) {
    outp(PNP_ADDRESS, reg);
    sys_ioDelay(1);
    outp(PNP_WRITE, val);
    sys_ioDelay(1);
}

static u8 pnp_readReg(u8 reg) {
    u8 ret;
    outp(PNP_ADDRESS, reg);
    sys_ioDelay(1);
    ret = inp(PNP_READ);
    sys_ioDelay(1);
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

static u16 pnp_readReg16(u8 reg) {
    return ((u16)pnp_readReg(reg) << 8) | ((u16)pnp_readReg(reg + 1));
}

/* Step 1: Send initiation key, puts all cards into config state */
static void pnp_sendInitKey(void) {
    u16 i;

    /* Write 0x00 twice to enter initiation state */
    outp(PNP_ADDRESS, 0x00);
    outp(PNP_ADDRESS, 0x00);

    /* Send the 32-byte LFSR key */
    for (i = 0; i < 32; i++) {
        outp(PNP_ADDRESS, pnp_initKey[i]);
        sys_ioDelay(1);
    }
}


static u8 pnp_readWithDelay() {
    u8 ret = inp(PNP_READ);
    sys_ioDelay(1);

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

/* Begin to enumerate currently unassigned device. Returns the Device ID if successful. */
static u32 pnp_startDeviceEnumeration(u8 csn) {
    u8 ourChecksum = 0x6A;
    u8 theirChecksum = 0;
    u32 id = 0UL;
    u16 i;

    /* Tell cards to begin isolation (serial ID read) */
    util_sleep(1);
    outp(PNP_ADDRESS, PNP_REG_ISOLATION);   /* set to Isolation register */
    util_sleep(1);

    /* Read 64-bit serial ID, one bit at a time */

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

    /* Assign CSN — card stays awake in config state,
        so we can read its config immediately */
    pnp_writeReg(PNP_REG_CSN, csn);

    return id;
}

static void pnp_populateDeviceInfo(pnp_DeviceInfo *device, u8 csn, u32 id) {
    pnp_DeviceInfo curdev;
    u16 logdev;

    memset(&curdev, 0, sizeof(curdev));

    curdev.csn           = csn;
    curdev.eisaId.dword  = id;
    curdev.numLogDevs = 0;
    
    pnp_decodeEisaId(curdev.eisaId, curdev.idStr);

    DBG("Device is: %08lx %s\n", SWAP32(curdev.eisaId.dword), curdev.idStr);

    for (logdev = 0; logdev < 4; logdev++) {
        pnp_LogicalDeviceInfo *curLogDev = &curdev.logDev[logdev];
        u16 j;
        bool is24 = false;
        bool is32 = false;

        pnp_writeReg(PNP_REG_LOGDEV, logdev);
        sys_ioDelay(1);

        curLogDev->active = pnp_readReg(PNP_REG_ACTIVATE);

        for (j = 0; j < 4; j++) { /* Read Mem32 */
            pnp_readStruct(&curLogDev->mem32[j], PNP_REG_MEM32(j), sizeof(pnp_Mem32Cfg));
            util_swapInPlace32(&curLogDev->mem32[j].base);
            util_swapInPlace32(&curLogDev->mem32[j].limitRange);
        }

        for (j = 0; j < 4; j++) { /* Read Mem24 */
            pnp_readStruct(&curLogDev->mem24[j], PNP_REG_MEM24(j), sizeof(pnp_Mem24Cfg));
            util_swapInPlace16(&curLogDev->mem24[j].base);
            util_swapInPlace16(&curLogDev->mem24[j].limitRange);
        }

        for (j = 0; j < 8; j++) {
            pnp_readStruct(&curLogDev->io[j], PNP_REG_IO(j), sizeof(pnp_IoCfg));
            util_swapInPlace16(&curLogDev->io[j].port);
        }

        for (j = 0; j < 2; j++) {
            pnp_readStruct(&curLogDev->irq[j], PNP_REG_IRQ(j), sizeof(pnp_IrqCfg));
        }

        for (j = 0; j < 2; j++) {
            pnp_readStruct(&curLogDev->dma[j], PNP_REG_DMA(j), sizeof(pnp_DmaCfg));
        }

        is32 |= curLogDev->mem32[0].base != 0UL;
        is24 |= curLogDev->mem24[0].base != 0;

        L866_ASSERTM(!(is24 && is32), "Logical device appars to be using 32 AND 24 bit mem descriptors");
        curLogDev->usesMem32 = is32;

        if (curLogDev->active) {
            curdev.numLogDevs++;
        }
    }

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

        pnp_populateDeviceInfo(&devices[numCards++], csn-1, id);
        
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
            pnp_populateDeviceInfo(dst, csn-1, id.dword);
            found = true;
            break;
        }

        pnp_writeReg(PNP_REG_WAKE_CSN, 0x00);  /* wake remaining unassigned */
    }

    pnp_writeReg(PNP_REG_CONFIG_CTRL, PNP_CTRL_WAIT_KEY );
    return found;
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
