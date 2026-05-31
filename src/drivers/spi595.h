#pragma once

#include <avr/io.h>
#include <stdint.h>

// ── 74xx595 chain over hardware SPI ─────────────────────────────────
// Pins: SER=MOSI(PB3), SRCLK=SCK(PB5), RCLK=latch(PB2/SS).
// DORD=1 (LSB-first) so each 595 maps QA=bit7 .. QH=bit0, matching the
// wiring 7:QA .. 0:QH. Existing CHARSET / digit bytes ship unchanged.

static uint8_t sr_seg = 0x00;   // segment register byte
static uint8_t sr_dig = 0x00;   // digit-select register byte

static inline void sr_tx(uint8_t b) { SPDR = b; while (!(SPSR & (1 << SPIF))); }

// Push both bytes then latch. NOTE: the chip FURTHEST down the chain must
// be sent FIRST. If seg/dig land on the wrong chip, swap these two lines.
void sr_flush(void) {
    sr_tx(sr_seg);
    sr_tx(sr_dig);
    PORTB |=  (1 << PB2);   // RCLK rising edge latches all bits
    PORTB &= ~(1 << PB2);
}

void spi595_init(void) {
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << PB2);   // MOSI, SCK, latch(SS) out
    SPCR  = (1 << SPE) | (1 << MSTR) | (1 << DORD); // enable, master, LSB-first, fosc/4
    sr_seg = 0; sr_dig = 0;
    sr_flush();
}
