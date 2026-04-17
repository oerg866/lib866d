/*  LIB866D
    CD-ROM Extensions interface

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef _CDEX_H_
#define _CDEX_H_

#include "types.h"

#include <stdlib.h>

#define CDA_MAX_TRACKS 99

/*  This library has a bit of an issue - I don't know enough about a CD's
    layout and structure to completely implement it properly.
    The length markers seem to have a 2 second offset and I cannot really figure
    out why. I think it has to do with buggy high sierra -> MM:SS:FF conversion.
    I also don't know if my method for figuring out track length is correct?
    This also means that currently, this library cannot be used to implement
    a CD player program with gapless playback :( */

#pragma pack(1)

/* CD-ROM/DA Track type */
typedef enum {
    tt_audio = 0,
    tt_data = 1,
    tt_4ch = 2,
    tt_invalid = 3
} cda_TrackType;

/* Red Book Position / length definition (MM:SS:FF) */
typedef struct {
    u8 frame;
    u8 second;
    u8 minute;
    u8 reserved;
} cda_MSF;

/* Track definition structure for TOC */
typedef struct {
    u8              trackId;    /* Actual logical track number on disc */
    cda_TrackType   type;       /* Audio / Data */
    cda_MSF         start;      /* Start marker */
    cda_MSF         length;     /* End marker */
} cda_TrackEntry;

/* CD table of contents structure (track listing + length info) */
typedef struct {
    u8              trackCount;
    u8              firstTrack;                 /* Track indices = 1-based, so the first track is 1 */
    u8              lastTrack;
    cda_MSF         totalDiscLength;
    cda_TrackEntry  tracks[CDA_MAX_TRACKS + 1]; /* index 0 = lead-out */
} cda_TOC;

/* CDEX info structure */
typedef struct {
    size_t  driveCount;
    char    firstDriveLetter;
    u8      verMajor;
    u8      verMinor;
} cdrom_CdexInfo;

#pragma pack()

/* CD-ROM Extensions */
/*  Checks if CD-ROM extensions (e.g. MSCDEX) are installed.
    info will contain the amount of CD-ROM Drives and the first drive's letter.
    info may be NULL for a simple installation check */
bool                    cdrom_getCdexInfo       (cdrom_CdexInfo *info);

/* CD-Audio */
/*  Get Audio CD Table of Contents. Returns true if successful. */
bool                    cda_getTOC              (char letter, cda_TOC *toc);
/*  Get array index of the first entry in a TOC that is a standard audio track
    returns U8_MAX if none found  */
u8                      cda_getFirstAudioTrack  (const cda_TOC *toc);
/*  Get pointer to the track of selected index */
const cda_TrackEntry   *cda_tocGetTrack         (const cda_TOC *toc, u8 track);

/*  Play selected audio track (toc array index) */
bool                    cda_playTrack           (char letter, const cda_TOC *toc, u8 track);
/*  Stop Audio CD Playback */
bool                    cda_stop                (char letter);
/*  Get playback position for given track, inTrack and onDisc may be NULL if the info is not needed.
    A return code of false is not necessarily fatal - it probably just means playback
    of the current track has ended. */
bool                    cda_getPlaybackPosition (char letter, cda_MSF *inTrack, cda_MSF *onDisc);

/* Converts MM:SS:FF structure to raw frame value */
u32                     cda_msfToFrames         (cda_MSF m);
/* Converts raw frame value to MM:SS:FF structure */
cda_MSF                 cda_framesToMSF         (u32 raw);

#endif
