/*  LIB866D
    Generic sound functions

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef _SND_H_
#define _SND_H_

#include "types.h"
#include "util.h"

#pragma pack(1)

/*  A structure depicting a generic mixer volume type.
    Sound code can use this internally or externally to define volume control. */
typedef struct {
    const char shortName[8];    /* Short (parameter) name */
    const char longName[16];    /* Long name */
    u8 volRegL;                 /* Codec register IDX for this volume (RIGHT) */
    u8 startBitL;               /* Start bit (0 = LSB) of the left channel */
    u8 volRegR;                 /* Codec register IDX for this volume (LEFT) */
    u8 startBitR;               /* Start bit (0 = LSB) of the left channel (0xFF = mono) */
    u8 width;                   /* Register bitfield width in bits */
    u8 minVal;                  /* Minimum value */
    u8 maxVal;                  /* Maximum value */
    bool volInvert;             /* If this is true, then the volume is treated as attenuation, not gain */
    u8 muteRegL;                /* Mute register IDX (0xFF = none) RIGHT */
    u8 muteBitL;                /* Mute bit L */
    u8 muteRegR;                /* Mute register IDX (0xFF = none) LEFT */
    u8 muteBitR;                /* Mute bit R (0xFF = mono) */
    bool muteInvert;            /* Mute bit is inverted (i.e. 0 = muted, 1 = unmuted) */
} snd_Volume;

typedef struct snd_VolumeControl snd_VolumeControl;

typedef bool (*snd_ReadFunc) (snd_VolumeControl *ctrl, size_t idx, u8 reg, u8 *val);
typedef bool (*snd_WriteFunc)(snd_VolumeControl *ctrl, size_t idx, u8 reg, u8 val);

typedef struct snd_VolumeControl {
    snd_ReadFunc read;
    snd_WriteFunc write;
    size_t volumeCount;
    snd_Volume *chans;
    void *userData;
} snd_VolumeControl;

typedef enum {
    ch_none = 0,
    ch_right,
    ch_left,
    ch_both,
} snd_ChannelMask;

#pragma pack()

/*  Get the absolute volume of a given channel in a volume control.
    l, r will contain the left and right side volumes
    muteL and muteR will indicate a mute bit (if it exists)
    l, r, muteL, muteR are optional */
bool snd_volumeGetAbs       (snd_VolumeControl *ctrl, size_t idx, u8 *l, u8 *r, bool *muteL, bool *muteR);

/*  Set the absolute volume of a given channel in a volume control. Affects channels given in <sides>. */
bool snd_volumeSetAbs       (snd_VolumeControl *ctrl, size_t idx, u8 value, snd_ChannelMask sides);

/* Emits a beep on the PC speaker with the given frequency */
void snd_beep               (u16 freq, u32 lengthMs);

// TODO bool snd_volumeSetPercent   (snd_VolumeControl *ctrl, size_t idx, float percent);

#endif
