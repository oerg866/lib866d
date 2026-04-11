/*  LIB866D
    Sound Blaster 16 Functions

    (C) 2026 E. Voirin (oerg866)
*/

#include "snd_sb16.h"

#include <conio.h>
#include <malloc.h>
#include <time.h>
#include <dos.h>
#include <string.h>

#include "sys.h"
#include "util.h"
#include "picdma.h"

#define __LIB866D_TAG__ "SB16"
#include "debug.h"

#define SB16_PORT_MIXER_CMD(base)       (base + 0x04)
#define SB16_PORT_MIXER_DATA(base)      (base + 0x05)
#define SB16_PORT_DSP_RESET(base)       (base + 0x06)
#define SB16_PORT_DSP_READ(base)        (base + 0x0A)
#define SB16_PORT_DSP_WRITE(base)       (base + 0x0C)
#define SB16_PORT_DSP_STATUS(base)      (base + 0x0E)
#define SB16_PORT_DSP_INT16_ACK(base)   (base + 0x0F)

/*  The DMA buffer will be allocated as one contiguous block
    but it is double buffered (halfway interrupt) */
#define SB16_BUFFER_SIZE        4096UL
#define SB16_BUFFER_SLICE_SIZE  (SB16_BUFFER_SIZE/2UL)

#define SB16_DSP_TIMEOUT_MS (500)

typedef enum {
    dc_SetTimeConst = 0x40,
    dc_SetRate          = 0x41,

    dc_DmaAuto16BitStart= 0xB6,
    dc_DmaAuto8BitStart = 0xC6,

    dc_SpeakerOn        = 0xD1,
    dc_SpeakerOff       = 0xD3,

    dc_Play8BitPause    = 0xD0,
    dc_Play8BitResume   = 0xD4,
    dc_Play16BitPause   = 0xD5,
    dc_Play16BitResume  = 0xD6,

    dc_DmaAuto16BitStop = 0xD9,
    dc_DmaAuto8BitStop  = 0xDA,

    dc_GetDspVersion    = 0xE1,
    dc_GetCopyright     = 0xE3,
} sb16_DSPCommand;

typedef enum {
    mr_Irq          = 0x80,
    mr_Dma          = 0x81,
    mr_IrqStatus    = 0x82,
} sb16_MixerReg;

static sys_DMABuffer    dmaBuffer;
static bool             initialized         = false;
static sys_ISR          oldISR              = NULL;
static SB16_DMACallback userCallback        = NULL;
static u16              bufferIndex         = 0;
static u16              sbPort              = 0;
static u16              sbIrq               = 0;;
static u16              sbDmaL              = 0;
static u16              sbDmaH              = 0;
static bool             oldIrqState         = false;
static u16              playbackDma         = 0;
static bool             atexitRegistered    = false;

#pragma pack(1)
typedef union { u8 raw; struct {
    u8 reserved0 : 4;   /* bits [3:0] - reserved, must be 0  */
    u8 isSigned  : 1;   /* bit  [4]   - 0=unsigned, 1=signed */
    u8 isStereo  : 1;   /* bit  [5]   - 0=mono,     1=stereo */
    u8 reserved1 : 2;   /* bits [7:6] - reserved, must be 0  */
};} sb16_DMAFormat;

typedef union { u8 raw; struct {
    u8 dma8     : 1;   /* bit [0] - 8-bit  DMA transfer IRQ pending  */
    u8 dma16    : 1;   /* bit [1] - 16-bit DMA transfer IRQ pending  */
    u8 midi     : 1;   /* bit [2] - MIDI   IRQ pending               */
    u8 reserved : 5;
};} sb16_IRQStatus;
#pragma pack()


static bool dspWaitReadReady(u16 io) {
    u32 timeoutTime = util_getTimeOffsetInClocks(SB16_DSP_TIMEOUT_MS);
    do { /* bit 7 = 0 means ready to write */
        if (inp(SB16_PORT_DSP_STATUS(io)) & 0x80) return true;
    } while (clock() < timeoutTime);

    DBG("DSP read timeout\n");
    return false;
}

/* Read the DSP data port */
static bool dspRead(u16 io, u8 *dst) {
    if (!dspWaitReadReady(io)) return false;

    *dst = inp(SB16_PORT_DSP_READ(io));
    return true;
}

/* UwU OwO */
static bool dspWaitWriteReady(u16 io) {
    u32 timeoutTime = util_getTimeOffsetInClocks(SB16_DSP_TIMEOUT_MS);

    do { /* bit 7 = 0 means ready to write */
        if (0 == (inp(SB16_PORT_DSP_WRITE(io)) & 0x80)) return true;
    } while (clock() < timeoutTime);

    DBG("DSP write timeout\n");
    return false;
}

/* Write the DSP data port */
static bool dspWrite(u16 io, u8 value) {
    if (!dspWaitWriteReady(io)) return false;

    outp(SB16_PORT_DSP_WRITE(io), value);
    DBG("DSP W %02x\n", value);
    return true;
}

/* Write the DSP data port */
static bool dspCmd(u16 io, sb16_DSPCommand cmd) {
    return dspWrite(io, (u8) cmd);
}

/* Reset the DSP */
static bool dspReset(u16 io) {
    bool ret;

    outp(SB16_PORT_DSP_RESET(io), 1);
    util_sleep(10);
    outp(SB16_PORT_DSP_RESET(io), 0);
    util_sleep(10);

    ret =  inp(SB16_PORT_DSP_STATUS(io)) & 0x80
        && inp(SB16_PORT_DSP_READ(io)) == 0xAA;

    DBG("dspReset %s\n", ret ? "ok" : "fail");

    return ret;
}

/* Write a mixer register */
static void mixerWrite(u16 io, sb16_MixerReg reg, u8 val) {
    outp(SB16_PORT_MIXER_CMD(io), (u8) reg);
    sys_ioDelay(10);
    outp(SB16_PORT_MIXER_DATA(io), val);
    sys_ioDelay(10);
}

/* Read a mixer register */
static u8 mixerRead(u16 io, sb16_MixerReg reg) {
    outp(SB16_PORT_MIXER_CMD(io), (u8) reg);
    sys_ioDelay(10);
    return inp(SB16_PORT_MIXER_DATA(io));
}

static bool setRate(u16 io, u16 rate) {
    return dspCmd(io, dc_SetRate)
        && dspCmd(io, (u8)(rate >> 8))
        && dspCmd(io, (u8)(rate & 0xFF));
}

static _inline void advancePlayback(void) {
    bufferIndex = bufferIndex ? 0 : 1;

    /* Next buffer needs to be filled by user! */
    if (userCallback != NULL) {
        u8 _huge *nextPtr = ((u8 _huge *) dmaBuffer.aligned);
        //not safe from isr context
        //DBG("advancePlayback buf %u cb %lp buf %lp\n", bufferIndex, userCallback, nextPtr);
        if (bufferIndex) nextPtr += ((u32) SB16_BUFFER_SLICE_SIZE);
        userCallback(nextPtr, SB16_BUFFER_SLICE_SIZE);
    }
}

static void _interrupt _far dmaPlaybackIsr(void) {
    sb16_IRQStatus status;
    status.raw = mixerRead(sbPort, mr_IrqStatus);

    /* If interrupt wasn't for us, chain & exit here. */
    if (!status.dma16) {
        _chain_intr(oldISR);
        return;
    }

    /*  Acknowledge this interrupt to the DSP first of all.. done by reading?! */
    inp(SB16_PORT_DSP_INT16_ACK(sbPort));

    /* We have exhausted this half of the buffer, fill next one. */
    advancePlayback();

    /* EOI */
    pic_irqAcknowledge(sbIrq);
}

bool sb16_init(u16 io, u16 irq, u16 dmaL, u16 dmaH) {
    bool success = true;
    u16 irqVector = pic_getVectorNumberForIRQ(irq);

    L866_ASSERTM(initialized == false, "SB16 component already initialized!");

    DBG("sb16_init start A%03x I%u D%u H%u\n", io, irq, dmaL, dmaH);

    if (!dspReset(io)) return false;

    DBG("DSP init ok\n");

    success &= sys_allocateDMABuffer(&dmaBuffer, SB16_BUFFER_SIZE);
    DBG("Alloc SB16 DMA buf (%s) buf: %lp aligned %lp\n", success ? "ok" : "fail", dmaBuffer.rawPtr, dmaBuffer.aligned);

    /* Save previous IRQ/ISR state */
    oldIrqState = pic_irqIsEnabled(irq);
    oldISR = _dos_getvect(irqVector);
    

    if (!success) {
        sb16_deinit();
        return false;
    }
    
    initialized = true;

    sbIrq = irq;
    sbPort = io;
    sbDmaL = dmaL;
    sbDmaH = dmaH;
    bufferIndex = 0;
    playbackDma = 0;

    /* Safety net */
    if (!atexitRegistered) {
        atexit(sb16_deinit);
        atexitRegistered = true;
    }

    /* Set new IRQ / ISR state */
    _dos_setvect(irqVector, dmaPlaybackIsr);
    pic_irqEnable(irq);

    DBG("irq %x vec %02x io %03x dma %u high %u isr %lp prev %lp\n", irq, irqVector, io, dmaL, dmaH, dmaPlaybackIsr, oldISR);

    return success;
}

void sb16_deinit() {
    if (initialized) {
        u16 i;

        sys_freeDMABuffer(&dmaBuffer);

        /* If the interrupt was previously disabled, return to this state. */
        if (oldIrqState == false) {
            pic_irqDisable(sbIrq);
        }

        /* Restore ISR for this irq */
        _dos_setvect(pic_getVectorNumberForIRQ(sbIrq), oldISR);
        initialized = false;
    }
}

static _inline u8 getIrqBitForIrq(u16 irq) {
    static const u8 irqBitLookup[16] = { 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 };
    if (irq > 15) return 0x00;
    return irqBitLookup[irq];
}

bool sb16_isIrqSupported(u16 irq) {
    return getIrqBitForIrq(irq) != 0x00;
}

sb16_DSPVersion sb16_getDSPVersion(u16 io) {
    sb16_DSPVersion ret = { 0xFF, 0xFF };
    bool success = true;

    L866_ASSERTM(initialized, "SB16 module not initialized.");

    if (!dspCmd(io, dc_GetDspVersion)) {
        return ret;
    }

    success &= dspRead(io, &ret.major);
    success &= dspRead(io, &ret.minor);

    return ret;
}

bool sb16_getDSPCopyright(u16 io, char *buf, u16 bufSize) {
    u16 i = 0;

    L866_ASSERTM(initialized, "SB16 module not initialized.");
    L866_NULLCHECK(buf);

    if (!dspReset(io))                  return false;
    if (!dspCmd(io, dc_GetCopyright))   return false;

    /* Read bytes until null terminator or buffer full */
    while (i < bufSize - 1) {
        u8  byte;

        if (!dspRead(io, &byte)) break;

        buf[i++] = (char)byte;

        if (byte == 0x00) break;
    }

    buf[i] = '\0';
    return true;
}

bool sb16_startPlayback16(u16 io, bool stereo, u16 rate, SB16_DMACallback cb) {
    u16 i;
    u8 irqBit = getIrqBitForIrq(sbIrq);
    sb16_DMAFormat fmt = {0};
    bool success = true;
    u16 playbackHalfSize = SB16_BUFFER_SLICE_SIZE;
    
    L866_ASSERTM(initialized, "SB16 module not initialized.");
    L866_NULLCHECK(cb);

    if (irqBit = 0x00) {
        DBG("Invalid IRQ: %u\n", sbIrq);
        return false;
    }

    if (!dspReset(io)) {
        DBG("DSP Reset failed\n");
        return false;
    }

    DBG("sb16_startPlayback16: Buffer aligned %lp\n", dmaBuffer.aligned);

    /* Prepare buffer */
    _fmemset(dmaBuffer.aligned, 0, (size_t)SB16_BUFFER_SIZE);
    bufferIndex = 0;

    /* Initial call to the callback to fill the buffer at the start */
    userCallback = cb;
    advancePlayback();

    /* Set sample rate */
    if (!setRate(io, rate)) return false;

    /* Playback size is in samples:
       if we are in stereo also half it */
    if (stereo) playbackHalfSize >>= 1; /* Stereo = twice the samples per... sample*/

    /* Set Format */
    fmt.isSigned = true;
    fmt.isStereo = stereo;
  
    /* Set IRQ and DMA channel in Mixer */
    playbackDma = sbDmaH;
    L866_ASSERTM(playbackDma != 0x04, "Invalid DMA");
    mixerWrite(io, mr_Irq, irqBit);
    mixerWrite(io, mr_Dma, BIT8(playbackDma));
    DBG("mixer irq bit %02x dma bit %02x\n", irqBit, BIT(playbackDma));

    /* Program the DMA */
    dma_dmaSetParams(playbackDma, dmaBuffer.aligned, (u16)SB16_BUFFER_SIZE);

    /* Start the stream */
    DBG("16bit pb start FMT %02x halfsize %02x\n", fmt.raw, playbackHalfSize);
    success &= dspCmd(io, dc_DmaAuto16BitStart);
    success &= dspWrite(io, fmt.raw);
    success &= dspWrite(io, (u8)((playbackHalfSize - 1) & 0xFF));
    success &= dspWrite(io, (u8)((playbackHalfSize - 1) >> 8));

    success &= dspCmd(io, dc_SpeakerOn);
    return success;
}

void sb16_stopPlayback(u16 io) {
    if (playbackDma != 0) {
        dma_dmaDisable(playbackDma);
        dspCmd(io, dc_Play16BitPause);
        dspCmd(io, dc_Play8BitPause);
        playbackDma = 0;
        dspReset(io);
    }
}
