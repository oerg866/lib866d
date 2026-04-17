/*  LIB866D
    CD-ROM Extensions interface

    (C) 2026 E. Voirin (oerg866)
*/

#include "cdex.h"

#define __LIB866D_TAG__ "CDEX"
#include "debug.h"

#include <dos.h>
#include <string.h>
#include <ctype.h>

#include "util.h"
#include "386asm.h"

/* Interrupt entry points for MSCDEX stuff */
#define MSCDEX_INSTALL      0x1500
#define MSCDEX_DRIVECHECK   0x150B
#define MSCDEX_VERSION      0x150C
#define MSCDEX_SEND_REQ     0x1510
#define MSCDEX_GET_DRIVES   0x150D

#define MSCDEX_REQ_INVALID_DRIVE    0x000F
#define MSCDEX_REQ_INVALID_FUNCTION 0x0001

/* All command codes for IOCTL INPUT commands */
typedef enum {
    i_getDeviceHeaderAddress = 0,
    i_getHeadLocation,
    i_reserved,
    i_getErrorStatistics,
    i_getAudioChannelInfo,
    i_readDriveBytes,
    i_getDeviceStatus,
    i_getSectorSize,
    i_getVolumeSize,
    i_getMediaChangeStatus,
    i_getAudioDiscInfo,
    i_getAudioTrackInfo,
    i_getQChannelInfo,
    i_getSubChannelInfo,
    i_getUpcCode,
    i_getAudioStatusInfo,
    ___INVALID_IOCTL_IN
} cdrom_IoctlInCmd;

/* All command codes for IOCTL OUTPUT commands */
typedef enum {
    o_ejectDisc = 0,
    o_lockDoor,
    o_resetDrive,
    o_audioChannelCtrl,
    o_controlString,
    o_closeTray,
    ___INVALID_IOCTL_OUT
} cdrom_IoctlOutCmd;

/* HSG / MSF Addressing mode */
typedef enum {
    m_hsg = 0,      /* High Sierra (sectors) */
    m_redbook = 1,  /* Red Book (MM:SS:FF) */
    ___INVALID_ADDRESSING_MODE
} cdrom_AdressingMode;

/* CDEX Requests Command Codes */
typedef enum {
    r_init              = 0,
    r_ioctlIn           = 3,
    r_ioctlOut          = 12,
    r_readLong          = 128,
    r_readLongPrefetch  = 130,
    r_seek              = 131,
    r_play              = 132,
    r_stop              = 133,
    r_writeLong         = 134,
    r_writeLongVerify   = 135,
    r_resume            = 136,
} cdrom_CdexRequestCmd;

#define CDAS_PLAYING        0x11
#define CDAS_PAUSED         0x12
#define CDAS_COMPLETE       0x13
#define CDAS_ERROR          0x14
#define CDAS_NODISC         0x15

#pragma pack(1)

/* CD Track Control Info Bitfield */
typedef struct {
    u8 reserved     : 4;
    u8 preEmphasis  : 1;
    u8 copyAllowed  : 1;
    u8 trackType    : 2; /* 0 = 2ch; 1 = data; 2 = 4ch; 3 = reserved */
} cdTrackCtrl;

/* Some IOCTLs use a mangled MSF struct.. ugh */
typedef struct {
    u8 minute;
    u8 second;
    u8 frame;
    u8 reserved;
} cda_MSFSwap;

/* CD IOCTL Request Structure */
typedef struct {
    u8 command; /* == cdrom_IoctlInCmd or cdrom_IoctlOutCmd */
    union {
        struct {
            u8          trackNumber;
            cda_MSF     start;
            cdTrackCtrl controlInfo;
        } audioTrackInfo;
        struct {
            u8          firstTrack;
            u8          lastTrack;
            cda_MSF     leadout;
        } audioDiscInfo;
        struct {
            u8          controlAdr;
            u8          currentTrack;
            u8          pointOrIndex;
            cda_MSFSwap timeInTrack;    /* Weirdly, here the values are shifted... */
            cda_MSFSwap timeOnDisc;
        } audioQChannelInfo;
    };
} cdrom_Ioctl;

/* CDEX Request Status Bitfield */
typedef union {
    u16 raw;
    struct {
        u16 error_code : 8;
        u16 done       : 1;
        u16 busy       : 1;
        u16 reserved   : 5;
        u16 error      : 1;
    };
} cdrom_CdexRequestStatus;

typedef union {
    cda_MSF msf;
    u32     hsg;
} CdPosition;

/* CDEX Request Structure */
typedef struct {
    u8  length;
    u8  unit;
    u8  command;    /* == cdrom_CdexRequestCmd */
    cdrom_CdexRequestStatus status;
    u8  reserved[8];
    union {
        struct {
            u8                  media;
            cdrom_Ioctl _far   *ctlBuf;
            u16                 byteCount;
            u16                 startingSector;
            void _far          *volId;
        } ioctl;
        struct {
            u8                  addressingMode; /* == cdrom_AdressingMode */
            CdPosition          start;  /*  MSF or HSG, depends on adressingMode */
            CdPosition          length; /*  MSF or HSG, depends on adressingMode 
                                            Correction: Upon actually using this,
                                            it seems like only this parameter
                                            has to be HSG. ????? */
        } play;
    };
} cdrom_CdexRequest;

#pragma pack()

/* Converts a drive letter to unit index expected by CDEX request */
static _inline u16 letterToIndex(char letter) { return tolower(letter) - 'a'; }

/* Checks if the given letter is a drive handled by CDEX */
static bool driveIsCdexDrive(char letter) {
    u16 driveIndex = letterToIndex(letter);
    u16 status = 0;
    u16 supported = 0;
    bool ret = false;

    _asm {
        mov ax, MSCDEX_DRIVECHECK
        mov cx, driveIndex
        int 0x2F
        mov status, bx
        mov supported, ax
    }

    L866_ASSERTM(status == 0xADAD, "bad CDEX state");

    ret = (supported != 0);

    DBG("driveIsCdexDrive %c: (idx %u) status %04x supported %04x ret %u \n", letter, driveIndex, status, supported, ret);

    return ret;
}

/* Converts MM:SS:FF structure to raw frame value */
u32 cda_msfToFrames(cda_MSF m) {
    return m.minute * 60UL * 75UL + m.second * 75UL + m.frame;
}

/* Converts raw frame value to MM:SS:FF structure */
cda_MSF cda_framesToMSF(u32 raw) {
    cda_MSF m;
    m.reserved = 0;
    m.frame = raw % 75UL;
    m.second = (u8) ((raw / 75UL) % 60UL);
    m.minute = (u8) (raw / 75UL / 60UL);
    return m;
}

/* Calculate distance between two MM:SS:FF points */
static cda_MSF calculateDistance(cda_MSF from, cda_MSF to) {
    u32 framesFrom = cda_msfToFrames(from);
    u32 framesTo = cda_msfToFrames(to);
    u32 distance;
    if (framesTo > framesFrom) {
        distance = framesTo - framesFrom;
    } else {
        distance = framesFrom - framesTo;
    }

    return cda_framesToMSF(distance);
}

/* Helper functions for High Sierra position info conversion */
static _inline u32       msfToHSG(cda_MSF m) { return cda_msfToFrames(m) - 150UL; }
static _inline cda_MSF   hsgToMSF(u32 hsg)   { return cda_framesToMSF(hsg + 150UL); }

/*  Get value for transfer size field of Input or Output IOCTL
    returns U16_MAX if invalid */
static u16 getIoctlTransferSize(u8 ioctlCmd, cdrom_CdexRequestCmd requestCmd) {
    if (requestCmd == r_ioctlIn) {
        switch((cdrom_IoctlInCmd) ioctlCmd) {
            case i_getDeviceHeaderAddress:  return 5;
            case i_getHeadLocation:         return 6;
            case i_reserved:                return 1;
            case i_getErrorStatistics:      return 1;
            case i_getAudioChannelInfo:     return 9;
            case i_readDriveBytes:          return 130;
            case i_getDeviceStatus:         return 5;
            case i_getSectorSize:           return 4;
            case i_getVolumeSize:           return 5;
            case i_getMediaChangeStatus:    return 2;
            case i_getAudioDiscInfo:        return 7;
            case i_getAudioTrackInfo:       return 7;
            case i_getQChannelInfo:         return 11;
            case i_getSubChannelInfo:       return 13;
            case i_getUpcCode:              return 11;
            case i_getAudioStatusInfo:      return 11;
            default:                        return U16_MAX;
        }
    } else if (requestCmd == r_ioctlOut) {
        switch((cdrom_IoctlOutCmd) ioctlCmd) {
            case o_ejectDisc:               return 1;
            case o_lockDoor:                return 2;
            case o_resetDrive:              return 1;
            case o_audioChannelCtrl:        return 9;
            case o_controlString:           return 1;
            case o_closeTray:               return 1;
            default:                        return U16_MAX;
        }
    } else {
        return U16_MAX;
    }
}

/*  Sends a request to the CDEX. */
static bool cdexRequest(char letter, cdrom_CdexRequest *req) {
    cdrom_CdexRequest _far *fReq = (cdrom_CdexRequest _far *) req;
    u16 reqSegment = FP_SEG(fReq);
    u16 reqOffset = FP_OFF(fReq);
    u16 index = letterToIndex(letter);
    u16 error = 0;

    req->length = sizeof(cdrom_CdexRequest);

    _asm {
        push es
        push bx
        mov ax, MSCDEX_SEND_REQ
        mov cx, index
        mov es, reqSegment
        mov bx, reqOffset
        int 0x2f
        pop bx
        pop es
        mov error, ax
    };

    DBG("cdexRequest %02x error %04x status %04x\n", req->command, error, req->status.raw);

    return error != MSCDEX_REQ_INVALID_DRIVE 
        && error != MSCDEX_REQ_INVALID_FUNCTION;
}

/*  Sends a CDEX request with the filled IOCTL Structure. RequestCmd depicts input or ouptut Ioctl.
    statusCode may be NULL and will receive the request status code, for further processing */
static bool cdexIoctl(char letter, cdrom_Ioctl *ctl, cdrom_CdexRequestCmd requestCmd, cdrom_CdexRequestStatus *statusCode) {
    cdrom_CdexRequest req;
    /*  MS-C bug: initializing unnamed unions writes two data blocks, smashing the stack,
        use memset instead */
    memset(&req, 0, sizeof(req));

    req.command         = requestCmd;
    req.ioctl.ctlBuf    = (cdrom_Ioctl _far *) ctl;
    req.ioctl.byteCount = getIoctlTransferSize(ctl->command, requestCmd);

    if (req.ioctl.byteCount == U16_MAX) {
        DBG("invalid ioctl command %u\n", (unsigned) ctl->command);
        return false;
    } 

    if (!cdexRequest(letter, &req)) return false;

    if (statusCode != NULL) *statusCode = req.status;

    DBG("IOCTL %c: cmd %02x, status %04x\n", letter, req.command, req.status);

    return req.status.error == 0 && req.status.done == 1;
}

/* Quick aliases for Input/Output IOCTLs */
static _inline bool cdexIoctlIn (char letter, cdrom_Ioctl *ctl, cdrom_IoctlInCmd cmd, cdrom_CdexRequestStatus *statusCode) { 
    ctl->command = (u8) cmd;
    return cdexIoctl(letter, ctl, r_ioctlIn, statusCode); 
}
static _inline bool cdexIoctlOut(char letter, cdrom_Ioctl *ctl, cdrom_IoctlOutCmd cmd, cdrom_CdexRequestStatus *statusCode) { 
    ctl->command = (u8) cmd;
    return cdexIoctl(letter, ctl, r_ioctlOut, statusCode); 
}

bool cdrom_getCdexInfo(cdrom_CdexInfo *info) {
    u16 _a;
    u16 driveCount;
    u16 firstDriveIndex;
    u8 minor = 0;
    u8 major = 0;

    _asm {
        mov ax, MSCDEX_INSTALL
        mov bx, 0
        int 0x2F
        mov _a, ax
        mov driveCount, bx
        mov firstDriveIndex, cx
        /* Do Version check, failure here is not critical... */
        mov ax, MSCDEX_VERSION
        mov bx, 0
        int 0x2F
        mov major, bh
        mov minor, bl
    };

    /* AL = 00 and BX = 00  -> not installed */
    if ((_a & 0xFF) == 0 && driveCount == 0) return false;

    if (info != NULL) {
        memset(info, 0, sizeof(cdrom_CdexInfo));
        info->driveCount = driveCount;
        info->firstDriveLetter = (char) ('a' + firstDriveIndex);
        info->verMajor = major;
        info->verMinor = minor;
    }

    DBG("int 2f 1500h ax %04x bx %04x cx %04x\n", _a, driveCount, firstDriveIndex);

    return true;
}

bool cda_getTOC(char letter, cda_TOC *toc) {
    cdrom_Ioctl ctl;
    u8 i;
    cda_MSF leadout;

    L866_NULLCHECK(toc);

    if (!cdrom_getCdexInfo(NULL)) return false;
    if (!driveIsCdexDrive(letter)) return false;
    if (!cdexIoctlIn(letter, &ctl, i_getAudioDiscInfo, NULL)) return false;

    leadout = ctl.audioDiscInfo.leadout;

    DBG("CD %c: Tracks %u - %u LeadOut %02um%02us%02uf\n",
        letter,
        ctl.audioDiscInfo.firstTrack, ctl.audioDiscInfo.lastTrack,
        leadout.minute, leadout.second, leadout.frame);

    toc->trackCount = 0;
    toc->firstTrack = ctl.audioDiscInfo.firstTrack;
    toc->lastTrack = ctl.audioDiscInfo.lastTrack;
    toc->totalDiscLength = ctl.audioDiscInfo.leadout;

    if (toc->lastTrack > CDA_MAX_TRACKS) {
        DBG("Warnung, CD has more than %u tracks! Truncating TOC\n", CDA_MAX_TRACKS);
        toc->lastTrack = CDA_MAX_TRACKS;
    }

    /* Now get the actual track infos */
    for (i = toc->firstTrack; i <= toc->lastTrack; i++) {
        cda_TrackEntry *t = &toc->tracks[toc->trackCount];
        cdrom_Ioctl trackCtl;
        
        trackCtl.audioTrackInfo.trackNumber = i;
        
        if (!cdexIoctlIn(letter, &trackCtl, i_getAudioTrackInfo, NULL)) return false;

        /* Now get all useful info we can */
        
        t->trackId = i;
        t->start = trackCtl.audioTrackInfo.start;
        t->type = (cda_TrackType) trackCtl.audioTrackInfo.controlInfo.trackType;

        /* This is the LAST track. Calculate its length by calculating the distance to the disc end */
        if (i == ctl.audioDiscInfo.lastTrack) {
            t->length = calculateDistance(t->start, ctl.audioDiscInfo.leadout);
        }
        
        /*  Now, except for the first track, we calculate the *previous* track's length by calculating the distance
            between the start of the previous track and the start of the *current* track */
        if (i > ctl.audioDiscInfo.firstTrack) {
            toc->tracks[toc->trackCount - 1].length = calculateDistance(toc->tracks[toc->trackCount - 1].start, t->start);
        }

        toc->trackCount++;
    }

    /* For debugging, print all track infos*/
#ifdef DEBUG
    for (i = toc->firstTrack; i <= toc->lastTrack; i++) {
        cda_TrackEntry *t = &toc->tracks[i - toc->firstTrack];
        DBG("Track %u: %s @ %02u:%02u.%02u, Length %02u:%02u.%02u\n", i,
            (t->type == tt_data) ? "DATA " : "AUDIO",
            t->start.minute, t->start.second, t->start.frame,
            t->length.minute, t->length.second, t->length.frame);
    }
#endif
    return true;
}

u8 cda_getFirstAudioTrack(const cda_TOC *toc) {
    u8 i;
    L866_NULLCHECK(toc);

    for (i = 0; i < toc->trackCount; i++) {
        if (toc->tracks[i].type == tt_audio) return i;
    }

    return U8_MAX;
}

const cda_TrackEntry *cda_tocGetTrack(const cda_TOC *toc, u8 track) {
    L866_NULLCHECK(toc);
    if (track >= toc->trackCount) return NULL;
    return &toc->tracks[track];
}

bool cda_playTrack(char letter, const cda_TOC *toc, u8 track) {
    cdrom_CdexRequest req;
    bool ret;

    L866_NULLCHECK(toc);

    if (!cdrom_getCdexInfo(NULL)) return false;
    if (!driveIsCdexDrive(letter)) return false;

    if (track >= toc->trackCount) {
        DBG("playTrack: %u out of range\n", track);
        return false;
    }

    req.command = r_play;
    req.play.addressingMode = (u8) m_redbook;
    req.play.start.msf = toc->tracks[track].start;
    /*  Full disclosure: I have absolutely no idea why 
        this (and only this) parameter is formatted that way. */
    req.play.length.hsg = msfToHSG(toc->tracks[track].length);

    if (!cdexRequest(letter, &req)) return false; 

    if (!req.status.busy) {
        DBG("playTrack: Request succeeded but BUSY flag didn't come up?!\n");
        return false;
    }

    return true;
}

bool cda_stop(char letter) {
    cdrom_CdexRequest req;
    req.command = r_stop;
    return cdexRequest(letter, &req);
}

bool cda_getPlaybackPosition(char letter, cda_MSF *inTrack, cda_MSF *onDisc) {
    cdrom_Ioctl ctl;
    cdrom_CdexRequestStatus status;

    if (!cdrom_getCdexInfo(NULL)) return false;
    if (!driveIsCdexDrive(letter)) return false;

    if (!cdexIoctlIn(letter, &ctl, i_getQChannelInfo, &status)) return false;

    /* Check if we're playing to begin with */

    if (!status.busy) {
        DBG("getPlaybackPosition: (%04x) no busy flag - probably not playing.\n", status.raw);
        return false;
    }

    /* For some reason these are byteswapped and also offset :| */

    if (inTrack != NULL) {
        inTrack->minute = ctl.audioQChannelInfo.timeInTrack.minute;
        inTrack->second = ctl.audioQChannelInfo.timeInTrack.second;
        inTrack->frame  = ctl.audioQChannelInfo.timeInTrack.frame;
    }
    if (onDisc != NULL) {
        onDisc->minute = ctl.audioQChannelInfo.timeOnDisc.minute;
        onDisc->second = ctl.audioQChannelInfo.timeOnDisc.second;
        onDisc->frame  = ctl.audioQChannelInfo.timeOnDisc.frame;
    }

    return true;
}
