/*  LIB866D
    RTC-based timer (IRQ8 / INT 70h)

    (C) 2026 E. Voirin (oerg866)
*/

#include "timer.h"
#include "picdma.h"
#include "sys.h"

#include <dos.h>
#include <stdlib.h>
#include <stddef.h>
#include <conio.h>

#define __LIB866D_TAG__ "TIMER"
#include "debug.h"

#define CMOS_INDEX      0x70
#define CMOS_DATA       0x71

#define REG_STATUS_A    0x0A
#define REG_STATUS_B    0x0B
#define REG_STATUS_C    0x0C

#define PIE_BIT         0x40    /* Periodic Interrupt Enable, Status B */
#define RTC_IRQ         8

/* ====================================================================
 *  Available RTC rates: rate N -> 32768 >> (N - 1) Hz
 *
 *   Rate 15 ->   2 Hz      Rate  8 ->  256 Hz
 *   Rate 14 ->   4 Hz      Rate  7 ->  512 Hz
 *   Rate 13 ->   8 Hz      Rate  6 -> 1024 Hz
 *   Rate 12 ->  16 Hz      Rate  5 -> 2048 Hz
 *   Rate 11 ->  32 Hz      Rate  4 -> 4096 Hz
 *   Rate 10 ->  64 Hz      Rate  3 -> 8192 Hz
 *   Rate  9 -> 128 Hz
 * ==================================================================== */

typedef struct {
    u8  rate;
    u16 frequency;
} rtcRateEntry;

static const rtcRateEntry rtcRates[] = {
    { 15,    2 }, { 14,    4 }, { 13,    8 }, { 12,   16 },
    { 11,   32 }, { 10,   64 }, {  9,  128 }, {  8,  256 },
    {  7,  512 }, {  6, 1024 }, {  5, 2048 }, {  4, 4096 },
    {  3, 8192 },
};

#define NUM_RATES (sizeof(rtcRates) / sizeof(rtcRates[0]))

static sys_ISR                  oldIsr              = NULL;
static bool                     irqWasEnabled       = false;
static timer_Callback           userCallback        = NULL;
static u8                       savedStatusB        = 0;
static u8                       activeRate          = 0;
static u16                      activeFreq          = 0;
static bool                     atexitRegistered    = false;

static u8 cmosRead(u8 reg) {
    u8 ret;
    outp(CMOS_INDEX, reg & 0x7F);
    sys_ioDelay(250);
    ret = inp(CMOS_DATA);
    sys_ioDelay(250);
    return ret;
}

static void cmosWrite(u8 reg, u8 val) {
    outp(CMOS_INDEX, reg & 0x7F);
    sys_ioDelay(250);
    outp(CMOS_DATA, val);
    sys_ioDelay(250);
}

static void _interrupt _far rtcIsr(void) {
    if (userCallback != NULL)
        userCallback();

    /* Re-arm RTC - MUST read Status C or no further ints fire */
    cmosRead(REG_STATUS_C);
    /* PIC EOI */
    pic_irqAcknowledge(RTC_IRQ);
}

/*
 * Find the lowest available frequency that is >= target.
 * Table is sorted ascending, so first match is the answer.
 */
static const rtcRateEntry *findBestRate(u16 target) {
    u8 i;

    for (i = 0; i < NUM_RATES; i++) {
        if (rtcRates[i].frequency >= target)
            return &rtcRates[i];
    }

    return NULL;
}

u16 timer_start(u16 desiredFrequency, timer_Callback cb) {
    const rtcRateEntry *best;
    u8 status;
    u8 statusBOut;
    u16 vec;

    L866_NULLCHECK(cb);
    L866_ASSERTM(activeFreq == 0, "timer already running");
    L866_ASSERTM(desiredFrequency >= 2, "minimum frequency is 2 Hz");

    best = findBestRate(desiredFrequency);
    L866_ASSERTM(best != NULL, "no suitable RTC rate found");

    userCallback = cb;
    vec = pic_getVectorNumberForIRQ(RTC_IRQ);

    _disable();

    irqWasEnabled = pic_irqIsEnabled(RTC_IRQ);

    /* Save and install ISR */
    oldIsr = _dos_getvect(vec);
    _dos_setvect(vec, rtcIsr);

    /* Enable periodic interrupt in Status B */
    savedStatusB = cmosRead(REG_STATUS_B);
    statusBOut = savedStatusB | PIE_BIT;
    cmosWrite(REG_STATUS_B, statusBOut);

    /* Set rate in Status A (preserve oscillator bits 7:4) */
    status = cmosRead(REG_STATUS_A);
    status = (status & 0xF0) | (best->rate & 0x0F);
    cmosWrite(REG_STATUS_A, status);

    /* Clear any pending interrupt */
    cmosRead(REG_STATUS_C);

    activeRate = best->rate;
    activeFreq = best->frequency;

    /* Safety net */
    if (!atexitRegistered) {
        atexit(timer_stop);
        atexitRegistered = true;
    }

    /* Enable IRQ8 in PIC */
    pic_irqEnable(RTC_IRQ);

    _enable();

    return activeFreq;
}

void timer_stop(void) {
    u16 vec;

    if (activeFreq == 0)
        return;

    vec = pic_getVectorNumberForIRQ(RTC_IRQ);

    _disable();
    cmosWrite(REG_STATUS_B, savedStatusB);          /* Restore Status B — disables PIE */
    cmosRead(REG_STATUS_C);                         /* Clear pending interrupt */
    _dos_setvect(vec, oldIsr);                      /* Restore old vector */
    if (!irqWasEnabled) pic_irqDisable(RTC_IRQ);    /* Disable IRQ8 if it was previously disabled */
    _enable();

    oldIsr        = NULL;
    userCallback  = NULL;
    activeRate    = 0;
    activeFreq    = 0;
}
