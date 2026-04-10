/*  LIB866D
    Generic sound functions

    (C) 2026 E. Voirin (oerg866)
*/

#include "snd.h"

#define __LIB866D_TAG__ "SND"
#include "debug.h"

#include "util.h"

static u16 getBitMask(u8 bitIdx, u8 width) {
    u16 ret = 0;

    while (width) {
        ret |= BIT(bitIdx);
        bitIdx++;
        width--;
    }
    return ret;
}

static _inline void insertBits(u8 *dst, u8 bitIdx, u8 width, u8 value) {
    u8 mask = getBitMask(bitIdx, width);
    *dst &= ~mask;
    *dst |= ((value << bitIdx) & mask);
}

static _inline u8 extractBits(u8 value, u8 bitIdx, u8 width) {
    u8 mask = getBitMask(bitIdx, width);
    return (value & mask) >> bitIdx;
}

bool snd_volumeGetAbs       (snd_VolumeControl *ctrl, size_t idx, u8 *l, u8 *r, bool *muteL, bool *muteR) {
    u8 volRegL = 0;
    u8 volRegR = 0;
    u8 muteRegL = 0;
    u8 muteRegR = 0;
    snd_Volume *vol;
    bool hasMuteRegL = false;
    bool hasMuteRegR = false;

    L866_NULLCHECK(ctrl);

    if (idx >= ctrl->volumeCount) {
        DBG("setVolume idx %zu out of range\n", idx);
        return false;
    }

    vol = &ctrl->chans[idx];

    if ((vol->volRegL  != 0xFF) && !ctrl->read(ctrl, idx, vol->volRegL,  &volRegL)) return false;
    if ((vol->volRegR  != 0xFF) && !ctrl->read(ctrl, idx, vol->volRegR,  &volRegR)) return false;
    if ((vol->muteRegL != 0xFF) && !ctrl->read(ctrl, idx, vol->muteRegL, &muteRegL)) return false;
    if ((vol->muteRegR != 0xFF) && !ctrl->read(ctrl, idx, vol->muteRegR, &muteRegR)) return false;

    if (l     && (vol->volRegL   != 0xFF) && (vol->startBitL != 0xFF)) *l     = extractBits(volRegL, vol->startBitL, vol->width);
    if (r     && (vol->volRegR   != 0xFF) && (vol->startBitR != 0xFF)) *r     = extractBits(volRegR, vol->startBitR, vol->width);

    /* If this is an attenuation, convert it now */
    if (vol->volInvert && l) *l = vol->maxVal - *l;
    if (vol->volInvert && r) *r = vol->maxVal - *r;

    if (muteL && (vol->muteRegL  != 0xFF) && (vol->muteBitL  != 0xFF)) *muteL = extractBits(muteRegL, vol->muteBitL, 1) ^ vol->muteInvert;
    if (muteR && (vol->muteRegR  != 0xFF) && (vol->muteBitR  != 0xFF)) *muteR = extractBits(muteRegR, vol->muteBitR, 1) ^ vol->muteInvert;

    return true;
}

static bool snd_volumeSetInternal(bool doChannel, bool doMute, snd_VolumeControl *ctrl, size_t idx, u8 volume, u8 volReg, u8 volStartBit, u8 volWidth, u8 muteReg, u8 muteStartBit, u8 muteValue) {
    bool ok = true;
    if (doChannel) {
        u8 vol = 0;
        u8 mute = 0;
        /* volume */
        if (!ctrl->read(ctrl, idx, volReg, &vol)) return false;
        insertBits(&vol, volStartBit, volWidth, volume);
        ok &= ctrl->write(ctrl, idx, volReg, vol);
        /* mute */
        if (doMute) {
            if (!ctrl->read(ctrl, idx, muteReg, &mute)) return false;
            insertBits(&mute, muteStartBit, 1, muteValue);
            ok &= ctrl->write(ctrl, idx, muteReg, mute);
        }

        DBG("vol (%zu) = %u, VolReg %02x=%02x MuteVal %u MuteReg %02x=%02x --> %u\n", 
            idx, volume, volReg, vol, muteValue, muteReg, mute, ok);

    }

    return ok;
}

bool snd_volumeSetAbs       (snd_VolumeControl *ctrl, size_t idx, u8 value, snd_ChannelMask sides) {
    u8 volRegL = 0;
    u8 volRegR = 0;
    u8 muteRegL = 0;
    u8 muteRegR = 0;
    snd_Volume *vol;
    bool hasMuteReg = false;
    u8 muteValue = (value == 0) ? 1 : 0;
    bool ok = true;
    bool l = sides == ch_left  || sides == ch_both;
    bool r = sides == ch_right || sides == ch_both;
    bool hasMuteL = false;
    bool hasMuteR = false;

    L866_NULLCHECK(ctrl);

    if (idx >= ctrl->volumeCount) {
        DBG("setVolume idx %zu out of range\n", idx);
        return false;
    }

    if (sides == ch_none) return false;

    vol = &ctrl->chans[idx];
 
    if (value > vol->maxVal) {
        DBG("setVolume %zu value %u out of range\n", idx, value);
        return false;
    }

    l &= (vol->volRegL != 0xFF) && vol->startBitL != 0xFF;
    r &= (vol->volRegR != 0xFF) && vol->startBitR != 0xFF;
    hasMuteL = l && (vol->muteRegL != 0xFF) && (vol->muteBitL != 0xFF);
    hasMuteR = r && (vol->muteRegR != 0xFF) && (vol->muteBitR != 0xFF);
    muteValue = vol->muteInvert ? !muteValue : muteValue;

    /* if this is an attenuation, convert it now */
    if (vol->volInvert) {
        value = vol->maxVal - value;
    }

    ok &= snd_volumeSetInternal(l, hasMuteL, ctrl, idx, value, vol->volRegL, vol->startBitL, vol->width, vol->muteRegL, vol->muteBitL, muteValue);
    ok &= snd_volumeSetInternal(r, hasMuteR, ctrl, idx, value, vol->volRegR, vol->startBitR, vol->width, vol->muteRegR, vol->muteBitR, muteValue);

    return ok;
}

void snd_beep(u16 freq, u32 lengthMs) {
    u16 divider;
    u8 dividerHi;
    u8 dividerLo;
    u8 port61;

    if (freq == 0 || lengthMs == 0UL) return;

    divider = (u16)(1193180UL / (u32) freq);

    /* read current speaker control */
    port61 = inp(0x61);

    /* turn off speaker gate while we reprogram the PIT */
    port61 &= ~0x03;
    outp(0x61, port61);

    /* program PIT channel 2: square wave, lobyte/hibyte */
    dividerHi = divider >> 8;
    dividerLo = divider & 0xFF;
    outp(0x43, 0xB6);           /* 10 11 011 0 */
    outp(0x42, dividerLo);      /* lo byte */
    outp(0x42, dividerHi);      /* hi byte */

    /* enable speaker gate + PIT output */
    port61 |= 0x03;
    outp(0x61, port61);

    /* wait */
    util_sleep(lengthMs);

    /* disable speaker */
    port61 &= ~0x03;
    outp(0x61, port61);
}
