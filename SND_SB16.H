/*  LIB866D
    Sound Blaster 16 Functions

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef __SND_SB16_H__
#define __SND_SB16_H__

#include "types.h"

#pragma pack(1)
typedef struct {
    u8 major;
    u8 minor;
} sb16_DSPVersion;
#pragma pack()

/*  SB16 DMA callback.
    dst is the target buffer, sizeBytes is the size in bytes to populate it with.
    the format depends on whatever was set up in startPlaybackXX.
    Runs in ISR context, be mindful of that. */
typedef void (*SB16_DMACallback)(void _huge *dst, u32 sizeBytes);

/*  Initialize SB16 environment for given IO Port, IRQ, Low/High DMA. High DMA may be 0! */
bool            sb16_init(u16 io, u16 irq, u16 dmaL, u16 dmaH);
/*  Deinitialize SB16 environment, restore IRQ / ISR state */
void            sb16_deinit(void);
/*  Checks if this IRQ would be supported by a SB16 card */
bool            sb16_isIrqSupported(u16 irq);

/* Note - the following functions require sb16_init to be called first. */

/*  Get DSP version */
sb16_DSPVersion sb16_getDSPVersion(u16 io);
/*  Get DSP copyright string */
bool            sb16_getDSPCopyright(u16 io, char *buf, u16 bufSize);
/*  Start a 16 bit DMA playback.
    'cb' will be called with a pointer to a buffer after an IRQ occurs, so that it can be
    filled with the apporpriate size */
bool            sb16_startPlayback16(u16 io, bool stereo, u16 rate, SB16_DMACallback cb);
/*  Stop a running DMA playback. */
void            sb16_stopPlayback(u16 io);

#endif
