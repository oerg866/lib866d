/*  LIB866D
    RTC-based timer (IRQ8)

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef __TIMER_H__
#define __TIMER_H__

#include "types.h"

typedef void (*timer_Callback)(void);

/*  Start periodic timer
    - desiredFrequency is the wanted rate (Hz, will be rounded UP to nearest RTC rate)
    - cb is called once per tick from ISR context. Must NOT be NULL.
    Returns actual frequency achieved in Hz, or 0 on failure.

    Uses RTC IRQ8 (NOT IRQ0 / PIT), so DOS clock, delay, etc. are unaffected.

    Available frequencies (Hz): 2, 4, 8, 16, 32, 64, 128, 256,
    512, 1024, 2048, 4096, 8192.
*/
u16 timer_start(u16 desiredFrequency, timer_Callback cb);

/*  Stop timer and fully restore RTC / interrupt state.
    This is added to the atexit() table when the first timer is started. */
void timer_stop(void);

#endif
