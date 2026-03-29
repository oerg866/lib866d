/*  LIB866D
    Interrupt Controller & DMA Controller Code

    (C) 2026 E. Voirin (oerg866)
*/

#ifndef _PIC_H_
#define _PIC_H_

#include "types.h"

/*  Checks if given interrupt is enabled. */
bool pic_irqIsEnabled(u16 irqLevel);

/*  Enables (unmasks) the given interrupt. 
    IRQs 8-15 will also unmask the IRQ2 (Chain) interrupt. */
void pic_irqEnable(u16 irqLevel);

/*  Disables (masks) the given interrupt.
    Note that pic_irqDisable on IRQ 8-15 DOES NOT mask IRQ2 on the master
    other devices on IRQ 9-15 might still need the cascade line open. */
void pic_irqDisable(u16 irqLevel);

/*  Acknowledges the given interrupt by sending EOI to the correct PICs. */
void pic_irqAcknowledge(u16 irqLevel);

/*  Gets the DOS IRQ vector number for a given IRQ level*/
u16 pic_getVectorNumberForIRQ(u16 irqLevel);

void dma_dmaDisable(u16 channel);
void dma_dmaEnable(u16 channel);
void dma_dmaSetParams(u16 channel, void _far *address, u16 size);

#endif
