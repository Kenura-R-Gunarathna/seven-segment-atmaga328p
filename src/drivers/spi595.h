#pragma once

#include <avr/io.h>
#include <stdint.h>

// ── 74xx595 chain over hardware SPI (ATmega32A) ─────────────────────
// ATmega32A SPI pins: MOSI=PB5, SCK=PB7, SS=PB4.
// DORD=1 (LSB-first): QA=bit7 .. QH=bit0.
// NOTE: In the function generator circuit U15 feeds the amplitude DAC (U14).
//       The display chain is kept for future use / compilation compatibility.

static uint8_t sr_seg = 0x00;
static uint8_t sr_dig = 0x00;

static inline void sr_tx(uint8_t b) { SPDR = b; while (!(SPSR & (1 << SPIF))); }

void sr_flush(void) {
    sr_tx(sr_seg);
    sr_tx(sr_dig);
    PORTB |=  (1 << PB4);
    PORTB &= ~(1 << PB4);
}

void spi595_init(void) {
    DDRB |= (1 << PB5) | (1 << PB7) | (1 << PB4);   // MOSI, SCK, latch
    SPCR  = (1 << SPE) | (1 << MSTR) | (1 << DORD);
    sr_seg = 0; sr_dig = 0;
    sr_flush();
}
