#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "386asm.h"
#include "vesabios.h"
#include "types.h"

bool vesa_getBiosInfo(vesa_BiosInfo *biosInfo) {
    vesa_BiosInfo _far *farOut = (vesa_BiosInfo _far *) biosInfo;
    u16 result = 0;

    if (biosInfo == NULL) {
        return false;
    }

    memset(biosInfo, 0, sizeof(vesa_BiosInfo));
    strcpy(biosInfo->signature, "VBE2");

    _asm {
        les di, farOut
        mov ax, 0x4F00
        int 0x10
        mov result, ax
    }

    if (result != 0x004F) {
        return false;
    }

    return vesa_isValidVesaBios(biosInfo);
}

bool vesa_isValidVesaBios(const vesa_BiosInfo *biosInfo) {
    if (biosInfo == NULL)
        return false;

    if (biosInfo->modeListPtr == (void _far *) NULL)
        return false;

    if (biosInfo->version.major == 0 && biosInfo->version.minor == 0)
        return false;

    if (0 != memcmp(biosInfo->signature, "VESA", 4)) {
        return false;
    }

    return true;
}

size_t vesa_getModeCount(const vesa_BiosInfo *biosInfo) {
    size_t count = 0;

    if (!vesa_isValidVesaBios(biosInfo)) {
        return 0;
    }

    while (biosInfo->modeListPtr[count] != 0xFFFF) {
        count++;
    }

    return count;
}

bool vesa_getModeInfoByModeId(vesa_ModeInfo *modeInfo, u16 modeId) {
    vesa_ModeInfo _far *modeInfoFarPtr = (vesa_ModeInfo _far *) modeInfo;
    u16 result = 0;

    if (modeInfo == NULL) {
        return false;
    }

    _asm {
        les di, modeInfoFarPtr
        mov ax, 0x4F01
        mov cx, modeId
        int 0x10
        mov result, ax
    }

    if (result != 0x004F) {
        return false;
    }

    return true;
}


bool vesa_getModeInfoByIndex(const vesa_BiosInfo *biosInfo, vesa_ModeInfo *modeInfo, size_t index) {
    if (modeInfo == NULL || biosInfo == NULL) {
        return false;
    }

    if (index >= vesa_getModeCount(biosInfo)) {
        return false;
    }

    return vesa_getModeInfoByModeId(modeInfo, biosInfo->modeListPtr[index]);
}

u32 vesa_getVRAMSize(const vesa_BiosInfo *biosInfo) {
    if (biosInfo == NULL) {
        return 0;
    }

    /* totalMemory is in 64KB blocks. */
    return biosInfo->totalMemory * 0x10000UL;
}
