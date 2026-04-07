/*  LIB866D
    Interrupt Controller & DMA Controller Code

    (C) 2026 E. Voirin (oerg866)
*/

#include "picdma.h"

#include <conio.h>

#include "sys.h"

#define __LIB866D_TAG__ "PICDMA"
#include "debug.h"

/* PIC ports */
#define PIC1_CMD                0x20
#define PIC1_DATA               0x21
#define PIC2_CMD                0xA0
#define PIC2_DATA               0xA1
#define PIC_EOI                 0x20    /* end-of-interrupt command */

#define BIT(x)                  (1 << (x))

/* IRQ 0-7  live on master PIC (0x21)
 * IRQ 8-15 live on slave  PIC (0xA1) */
static u8 pic_getMaskPortAndBit(u16 irqLevel, u16 *maskPort) {
    if (irqLevel >= 8) {
        *maskPort = PIC2_DATA;
        return BIT(irqLevel - 8);
    } else {
        *maskPort = PIC1_DATA;
        return BIT(irqLevel);
    }
}

bool pic_irqIsEnabled(u16 irqLevel) {
    u16 maskPort;
    u8  bit = pic_getMaskPortAndBit(irqLevel, &maskPort);

    /* bit=0 in IMR means enabled, bit=1 means masked */
    return !(inp(maskPort) & bit);
}

void pic_irqEnable(u16 irqLevel) {
    u16 maskPort;
    u8  bit  = pic_getMaskPortAndBit(irqLevel, &maskPort);
    u8  mask = inp(maskPort) & ~bit;    /* clear the bit to unmask */

    outp(maskPort, mask);

    /* IRQ 8-15 also need IRQ2 (cascade line) unmasked on master */
    if (irqLevel >= 8) {
        pic_irqEnable(2);
    }
}

void pic_irqDisable(u16 irqLevel) {
    u16 maskPort;
    u8  bit  = pic_getMaskPortAndBit(irqLevel, &maskPort);
    u8  mask = inp(maskPort) | bit;     /* set the bit to mask */

    outp(maskPort, mask);
}

void pic_irqAcknowledge(u16 irqLevel) {
    /* Send EOI to SLAVE PIC if applicable */
    if (irqLevel >= 8)
        outp(PIC2_CMD, PIC_EOI);
    
    /* Send EOI to MASTER PIC */
    outp(PIC1_CMD, PIC_EOI);
}

u16 pic_getVectorNumberForIRQ(u16 irqLevel) {
    if (irqLevel >= 8) {
        return irqLevel - 8 + 0x70;
    } else {
        return irqLevel + 8;
    }
}

/*
 * DMA channel register map
 * Channels 0-3: 8-bit  (master DMA controller, base 0x00)
 * Channels 4-7: 16-bit (slave  DMA controller, base 0xC0)
 *
 * For 16-bit channels, addresses and counts are in WORDS,
 * and port addresses are doubled (each register is 2 bytes apart).
 */

/* 8-bit DMA controller ports */

/* 16-bit DMA controller ports */

#define DMA8_MASK_REG       0x0A
#define DMA8_MODE_REG       0x0B
#define DMA8_FLIPFLOP_REG   0x0C

#define DMA16_MASK_REG      0xD4
#define DMA16_MODE_REG      0xD6
#define DMA16_FLIPFLOP_REG  0xD8

/* Mode byte: auto-init, read (card reads from memory), channel bits */
#define DMA_MODE_AUTOINIT   0x58    /* bits[7:6]=01 single, bit[4]=1 auto-init, bit[3]=0 read */

/* Returns the 0-3 index within the controller for this channel */
#define IS16BIT(x) ((x) >= 4)

static const u16 addrPorts[]    = { 0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC };
static const u16 pagePorts[]    = { 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A };
static const u16 countPorts[]   = { 0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE };

static u16 dma_addrPort      (u16 ch) { return addrPorts[ch]; }
static u16 dma_pagePort      (u16 ch) { return pagePorts[ch]; }
static u16 dma_countPort     (u16 ch) { return countPorts[ch]; }
static u16 dma_maskPort      (u16 ch) { return IS16BIT(ch) ? DMA16_MASK_REG : DMA8_MASK_REG; }
static u16 dma_flipflopPort  (u16 ch) { return IS16BIT(ch) ? DMA16_FLIPFLOP_REG : DMA8_FLIPFLOP_REG; }
static u16 dma_modePort      (u16 ch) { return IS16BIT(ch) ? DMA16_MODE_REG : DMA8_MODE_REG; }

static _inline u16 dma_channelIndex(u16 channel) { return channel & 0x03; }

void dma_dmaDisable(u16 channel) {
    /* bit2=1 masks the channel */  
    outp(dma_maskPort(channel), 0x04 | dma_channelIndex(channel));
}

void dma_dmaEnable(u16 channel) {
    /* bit2=0 unmasks the channel */
    outp(dma_maskPort(channel), dma_channelIndex(channel));
}

void dma_dmaSetParams(u16 channel, void _far *address, u16 size) {
    u16 idx         = dma_channelIndex(channel);
    u32 physAddr    = sys_getPhysicalAddress(address);
    u8  page        = (u8) (physAddr >> 16);

    dma_dmaDisable(channel);

    if (IS16BIT(channel)) {
        physAddr    >>= 1;
        size        >>= 1;
    }

    outp(dma_flipflopPort(channel), 0x00),
    outp(dma_modePort(channel),     DMA_MODE_AUTOINIT | idx),
    outp(dma_addrPort(channel),     (u8) (physAddr >>  0));
    outp(dma_addrPort(channel),     (u8) (physAddr >>  8));
    outp(dma_pagePort(channel),     page);
    outp(dma_countPort(channel),    (u8) ((size-1) >>  0));
    outp(dma_countPort(channel),    (u8) ((size-1) >>  8));

    dma_dmaEnable(channel);

    DBG("dmaSetParams: ch %u @ %lp phys %08lx 0x%04x bytes\n", channel, address, physAddr, size);
}
