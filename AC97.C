#include "ac97.h"

/*  LIB866D
    AC'97 Mixer Functions

    (C) 2025 E. Voirin (oerg866)
*/

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "sys.h"
#include "util.h"

#define __LIB866D_TAG__ "AC97"
#include "debug.h"

#define AC97_REG_GENERAL        (0x20)
#define AC97_REG_GENERAL_3D_ON  (0x2000)
#define AC97_REG_PWR_STATUS     (0x26)
#define AC97_REG_EXTENDED_CTRL  (0x2A)
#define AC97_REG_DAC_RATE       (0x2C)
#define AC97_REG_ADC_RATE       (0x32)
#define AC97_REG_VENDOR_ID1     (0x7C)
#define AC97_REG_VENDOR_ID2     (0x7E)

/* Ordering: See ac97.h */
static const ac97_codecVolumeRegister c_ac97_codecVolumeRegisters[] = {
    { "Master" ,    0x02, false, 15, -1, 0x1F, 0, 0, 0 },
    { "Wave Out",   0x18, false, 15, -1, 0x1F, 0, 0, 0 },
    { "PC Speaker", 0x0A, true,  15, -1, 0x0F, 1, 0, 0 },
    { "Microphone", 0x0E, true,  15,  6, 0x1F, 0, 0, 0 },
    { "Line In",    0x10, false, 15, -1, 0x1F, 0, 0, 0 },
    { "CD Audio",   0x12, false, 15, -1, 0x1F, 0, 0, 0 },
    { "Video In",   0x14, false, 15, -1, 0x1F, 0, 0, 0 },
    { "Auxiliary",  0x16, false, 15, -1, 0x1F, 0, 0, 0 },
    { "Line 2",     0x04, false, 15, -1, 0x1F, 0, 0, 0 },
};

/* From Linux kernel sources & some datasheets i found */
static const struct {
    const u32   codecId;
    const u32   mask;
    const char *name;
} c_ac97_supportedCodecs[] = {
    { 0x434d4941UL, 0xffffffffUL, "C-Media CMI9738" },
    { 0x434d4961UL, 0xffffffffUL, "C-Media CMI9739" },
    { 0x434d4969UL, 0xffffffffUL, "C-Media CMI9780" },
    { 0x434d4978UL, 0xffffffffUL, "C-Media CMI9761A" },
    { 0x434d4982UL, 0xffffffffUL, "C-Media CMI9761B" },
    { 0x434d4983UL, 0xffffffffUL, "C-Media CMI9761A+" },
    { 0x43525900UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4297" },
    { 0x43525910UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4297A" },
    { 0x43525920UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4298" },
    { 0x43525928UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4294" },
    { 0x43525930UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4299" },
    { 0x43525948UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4201" },
    { 0x43525958UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4205" },
    { 0x43525960UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4291" },
    { 0x43525970UL, 0xfffffff8UL, "Cirrus Logic/Crystal CS4202" },
    { 0x83847600UL, 0xffffffffUL, "SigmaTel STAC9700" },
    { 0x83847601UL, 0xffffffffUL, "SigmaTel STAC9701" },
    { 0x83847605UL, 0xffffffffUL, "SigmaTel STAC9704" },
    { 0x83847608UL, 0xffffffffUL, "SigmaTel STAC9708" },
    { 0x414c4300UL, 0xffffff00UL, "Avance Logic ALC100" }, 
    { 0x414c4710UL, 0xfffffff0UL, "Avance Logic ALC200" }, 
    { 0x414c4730UL, 0xffffffffUL, "Avance Logic ALC101" }, 
    { 0x414c4740UL, 0xfffffff0UL, "Avance Logic ALC202" },
    { 0x49434501UL, 0xffffffffUL, "IC Ensemble ICE1230 / VIA VT1611" },
    { 0x49434511UL, 0xffffffffUL, "IC Ensemble ICE1232 / VIA VT1611A" },
    { 0x49434514UL, 0xffffffffUL, "IC Ensemble ICE1232A" },
    { 0x49434551UL, 0xffffffffUL, "IC Ensemble ICE1232A" },
    { 0x56494120UL, 0xfffffff0UL, "VIA VT1613" },
    { 0x56494141UL, 0xffffffffUL, "VIA VT1612" },
    { 0x56494161UL, 0xffffffffUL, "VIA VT1612A" },
    { 0x00000000UL, 0x00000000UL, "Unknown/Generic" },
};

static void ac97_mixerGetOne(ac97_Interface *ac, u16 volumeIndex) {
    ac97_codecVolumeRegister *vol = &ac->mixer[volumeIndex];
    u16 val = ac->read(ac->dev, vol->reg);

    DBG("mixerGetOne: reg %02x val %04x\n", vol->reg, val);

    vol->vol_muted = false;
    vol->vol_boost = false;

    /* Check mute bit, mask it out if present */
    if (vol->muteBit >= 0) {
        vol->vol_muted = (val >> vol->muteBit) & 1;
        val &= ~(1 << vol->muteBit);
    }

    /* Check 20dB boost bit, mask it out if present */
    if (vol->boostBit >= 0) {
        vol->vol_boost = (val >> vol->boostBit) & 1;
        val &= ~(1 << vol->boostBit);
    }

    vol->vol_r = (val & 0x7F) >> vol->attenuationShift;
    vol->vol_l = val >> (8 + vol->attenuationShift);
    
    /* Mono channel: L = R */
    if (vol->mono) {
        vol->vol_l = vol->vol_r;
    }


    L866_ASSERTM(vol->vol_r <= vol->maxAttenuation, "Unexpected out of range volume!");
    L866_ASSERTM(vol->vol_l <= vol->maxAttenuation, "Unexpected out of range volume!");

    /* turn attenuation to volume by inverting it */
    vol->vol_r = vol->maxAttenuation - vol->vol_r;
    vol->vol_l = vol->maxAttenuation - vol->vol_l;    
}

bool ac97_mixerInit(ac97_Interface *ac, codecReadFunc readFunc, codecWriteFunc writeFunc, void *dev) {
    u16 i;

    L866_NULLCHECK(ac);
    L866_NULLCHECK(readFunc);
    L866_NULLCHECK(writeFunc);

    /* Init r/w funcs and device specific structure */
    ac->read = readFunc;
    ac->write = writeFunc;
    ac->dev = dev;

    /* Attempt to power up the AC97 codec... */
    if (!ac97_powerUp(ac)) {
        return false;
    }

    memcpy(ac->mixer, c_ac97_codecVolumeRegisters, sizeof(c_ac97_codecVolumeRegisters));

    for (i = 0; i < (u16) __AC97_VOL_COUNT__; i++) {
        ac97_mixerGetOne(ac, i);
    }

    return true;
}

/* Writes a value to the codec and verifies it is written correctly by reading it back */
static bool ac97_writeVerify(ac97_Interface *ac, u16 reg, u16 value) {
    DBG("Codec Write: %04x -> %04x\n", reg, value);
    ac->write(ac->dev, reg, value);
    return value == ac->read(ac->dev, reg);
}

u32 ac97_getCodecId(ac97_Interface *ac) {
    u32 id = 0;
    L866_NULLCHECK(ac);

    id = ac->read(ac->dev, AC97_REG_VENDOR_ID1);
    id <<= 16UL;
    id |= ac->read(ac->dev, AC97_REG_VENDOR_ID2);
    return id;
}

const char *ac97_getCodecName(ac97_Interface *ac) {
    u32 codecId = ac97_getCodecId(ac);
    u16 i;

    for (i = 0; i < ARRAY_SIZE(c_ac97_supportedCodecs); i++) {
        u32 maskedId = codecId & c_ac97_supportedCodecs[i].mask;
        if (maskedId == c_ac97_supportedCodecs[i].codecId) {
            return c_ac97_supportedCodecs[i].name;
        }
    }

    L866_ASSERTM(false, "Fatal error in codec name list check");
    return NULL;
}

bool ac97_powerUp(ac97_Interface *ac) {
    i16 timeout = 1000;

    ac->write(ac->dev, AC97_REG_PWR_STATUS, 0x0000);

    while (timeout--) {
        /* Codec not in power down state & all IFs up? */
        if (0x000F == ac->read(ac->dev, AC97_REG_PWR_STATUS)) {
            return true;
        }
        /* If not, delay and truck on */
        sys_ioDelay(1);
    }

    DBG("ac97 codec powerup timeout");

    return false;
}

bool ac97_getSurround(ac97_Interface *ac) {
    u16 reg; 
    L866_NULLCHECK(ac);

    reg = ac->read(ac->dev, AC97_REG_GENERAL);
    reg &= AC97_REG_GENERAL_3D_ON;

    return reg != 0;
}

bool ac97_setSurround(ac97_Interface *ac, bool enable) {
    u16 reg; 
    L866_NULLCHECK(ac);

    reg = ac->read(ac->dev, AC97_REG_GENERAL);
    reg &= ~AC97_REG_GENERAL_3D_ON;
    reg |= enable ? AC97_REG_GENERAL_3D_ON : 0;
    return ac97_writeVerify(ac, AC97_REG_GENERAL, reg);
}

bool ac97_setMicBoost(ac97_Interface *ac, bool enable) {
    u16 reg;
    L866_NULLCHECK(ac);

    reg = ac->read(ac->dev, ac->mixer[AC97_VOL_MIC].reg);
    reg &= ~(1 << ac->mixer[AC97_VOL_MIC].boostBit);
    reg |= ((u16) enable) << ac->mixer[AC97_VOL_MIC].boostBit;

    return ac97_writeVerify(ac, ac->mixer[AC97_VOL_MIC].reg, reg);
}

const char *ac97_getChannelName(ac97_Interface *ac, ac97_VolumeCtrlIdx channel) {
    L866_NULLCHECK(ac);

    if (channel > __AC97_VOL_COUNT__) {
        return "???";
    }

    return ac->mixer[channel].name;
}

void ac97_getVolume(ac97_Interface *ac, ac97_VolumeCtrlIdx channel, ac97_Volume *vol) {
    L866_NULLCHECK(ac);
    L866_NULLCHECK(vol);

    /* Update internal mixer structure */
    ac97_mixerGetOne(ac, channel);

    /* Assign outgoing values */
    vol->maxVol = ac->mixer[channel].maxAttenuation;
    vol->l = ac->mixer[channel].vol_l;
    vol->r = ac->mixer[channel].vol_r;
    vol->l_percent = ((float) vol->l * 100.0f) / (float) vol->maxVol;
    vol->r_percent = ((float) vol->r * 100.0f) / (float) vol->maxVol;
    vol->muted = ac->mixer[channel].vol_muted;
}

bool ac97_setVolume(ac97_Interface *ac, ac97_VolumeCtrlIdx channel, u16 l, u16 r, bool mute) {
    bool ret = true;
    ac97_codecVolumeRegister *vol;
    u16 value = 0;
    u16 extraBitsMask = 0;

    L866_NULLCHECK(ac);

    DBG("Channel %u L %u R %u mute %u\n", (unsigned) channel, l, r, mute);

    vol = &ac->mixer[channel];

    /* Check ovolumes are in range */
    if (l > vol->maxAttenuation || r > vol->maxAttenuation) {
        DBG("Value out of range\n");
        return false;
    }

    /* Check mono */
    if (vol->mono && l != r) {
        DBG("Attempted stereo volume setting on mono channel\n");
    }
    
    /* Check muting when reg has no mute bit */
    if (mute && (vol->muteBit < 0)) {
        DBG("Ignoring mute flag on volume control that doesn't support it\n");
    }
    
    /* Turn volume back into attenutation */
    l = vol->maxAttenuation - l;
    r = vol->maxAttenuation - r;

    /* Get existing value and mask off extra bits we know (atm just 20db boost if it's a mic channel) */
    if (vol->boostBit >= 0) {
        extraBitsMask |= (1 << vol->boostBit);
    }

    value = ac->read(ac->dev, vol->reg);
    value &= extraBitsMask;

    /* Funnel into register */
    value |= r << vol->attenuationShift;
    
    if (!vol->mono) {
        value |= l << (8 +vol->attenuationShift);
    }

    if (vol->muteBit >= 0) {
        value &= ~(1 << vol->muteBit);
        value |= ((u16) mute) << vol->muteBit;
    }

    /* Write the value back */
    ret = ac97_writeVerify(ac, vol->reg, value);

    /* Update internal mixer struct */
    ac97_mixerGetOne(ac, channel);
    return ret;
}

bool ac97_setVolumePercent(ac97_Interface *ac, ac97_VolumeCtrlIdx channel, float l, float r, bool mute) {
    u16 lInt = 0;
    u16 rInt = 0;

    L866_NULLCHECK(ac);

    lInt = (u16) util_round(l * (float) ac->mixer[channel].maxAttenuation / 100.0f);
    rInt = (u16) util_round(r * (float) ac->mixer[channel].maxAttenuation / 100.0f);

    return ac97_setVolume(ac, channel, lInt, rInt, mute);
}


bool ac97_setVariableSampleRate(ac97_Interface *ac, bool enable, u16 rate) {
    u16 extendedCtrlReg;
    
    L866_NULLCHECK(ac);
    
    /* Enable VSR */
    extendedCtrlReg = ac->read(ac, AC97_REG_EXTENDED_CTRL);
    extendedCtrlReg &= 0xFFFE;
    extendedCtrlReg |= (u16) enable;
        
    /* If VSR bit doesn't stick, codec doesn't support it */
    if (!ac97_writeVerify(ac, AC97_REG_EXTENDED_CTRL, extendedCtrlReg)) {
        printf("Fail: %04x got %04x\n", extendedCtrlReg, ac->read(ac, AC97_REG_EXTENDED_CTRL));
        return false;
    }

    if (!enable) {        
        return true;
    }
    
    return ac97_writeVerify(ac, AC97_REG_DAC_RATE, rate);
}
