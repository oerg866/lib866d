#ifndef _AC97_H_
#define _AC97_H_

/*  LIB866D
    AC'97 Mixer Functions

    (C) 2025 E. Voirin (oerg866)
*/

#include "types.h"

typedef enum {
    AC97_VOL_MASTER = 0,
    AC97_VOL_WAVE,
    AC97_VOL_PCSPK,
    AC97_VOL_MIC,
    AC97_VOL_LINEIN,
    AC97_VOL_CDIN,
    AC97_VOL_VIDEO,
    AC97_VOL_AUX,
    __AC97_VOL_COUNT__
} ac97_VolumeCtrlIdx;

typedef struct {
    const char *name;               /* Name of this volume control */
    u16         reg;                /* Register index that controls this volume */
    bool        mono;               /* This volume is mono (R value only) */
    i16         muteBit;            /* bit index for muting (-1 = not supported) */
    i16         boostBit;           /* bit index for 20dB mic bost (-1 = not supported) */
    u16         maxAttenuation;     /* value representing maximum attenuation */
    u16         attenuationShift;   /* The amount the attenuation value is shifted to the left in the register */
    u16         vol_l;              /* Current Left 0-maxAttenuation volume value (i.e. maxAttenuation minus attenuation) */
    u16         vol_r;              /* Current Right 0-maxAttenuation volume value (i.e. maxAttenuation minus attenuation) */
    bool        vol_muted;
    bool        vol_boost;
} ac97_codecVolumeRegister;

typedef struct {
    u16 maxVol;
    u16 l;
    u16 r;
    float l_percent;
    float r_percent;
    bool muted;
} ac97_Volume;

typedef void (*codecWriteFunc)(void *dev, u16 reg, u16 value);
typedef u16  (*codecReadFunc) (void *dev, u16 reg);

typedef struct ac97_Interface_s {
    codecReadFunc read;         /* Device specific function to read AC97 codec register */
    codecWriteFunc write;       /* Device specific function to write AC97 codec register */
    void *dev;                  /* Device specific structure */
    ac97_codecVolumeRegister mixer[__AC97_VOL_COUNT__]; /* Mixer. You MUST call ac97_mixerInit! */
} ac97_Interface;

/*  Initialize AC97 mixer interface.
    'read', 'write' and 'dev' members will be populated and the codec will be powered up.
    The internal mixer structure will contain all current volume settings. */
bool ac97_mixerInit(ac97_Interface *ac, codecReadFunc readFunc, codecWriteFunc writeFunc, void *dev);
/*  Get the Vendor/Device ID code of the codec */
u32 ac97_getCodecId(ac97_Interface *ac);
/*  Get the name of the codec (if the library knows it) */
const char *ac97_getCodecName(ac97_Interface *ac);
/*  Power up the codec, disabling all power saving bits (note: mixerInit already does this) */
bool ac97_powerUp(ac97_Interface *ac);
/*  Get status of 3D Surround Sound */
bool ac97_getSurround(ac97_Interface *ac);
/*  Enable/Disable 3D Surround Sound */
bool ac97_setSurround(ac97_Interface *ac, bool enable);
/*  Set the Mic boost for a given channel */
bool ac97_setMicBoost(ac97_Interface *ac, bool enable);
/*  Get name of a given volume channel */
const char *ac97_getChannelName(ac97_Interface *ac, ac97_VolumeCtrlIdx channel);
/*  Get the volume for a given channel */
void ac97_getVolume(ac97_Interface *ac, ac97_VolumeCtrlIdx channel, ac97_Volume *vol);
/*  Set the volume for a given channel */
bool ac97_setVolume(ac97_Interface *ac, ac97_VolumeCtrlIdx channel, u16 l, u16 r, bool mute);
bool ac97_setVolumePercent(ac97_Interface *ac, ac97_VolumeCtrlIdx channel, float l, float r, bool mute);
/*  Set Variable Sample Rate feature for the DAC of the codec */
bool ac97_setVariableSampleRate(ac97_Interface *ac, bool enable, u16 rate);
#endif
