#pragma once

#include <avr/io.h>
#include <stdint.h>

/*
 * 16-bit R-2R DAC via 2× 74HC595, hardware SPI.
 *
 * Wiring (ATmega32A):
 *   PB5 (MOSI) → IC1 DS  (serial data in)
 *   PB7 (SCK)  → IC1/IC2 SRCLK  (shift clock)
 *   PB3        → IC1/IC2 RCLK   (latch — separate from PB4 used by other chain)
 *   IC1 Q7'    → IC2 DS  (daisy chain: IC1 high byte, IC2 low byte)
 *   IC1 Q7     → VOUT  (MSB end of R-2R ladder)
 *   IC2 Q0 → [20kΩ] → GND  (2R termination at LSB end)
 *
 * SPI: MSB-first, mode 0, f/4 = 4 MHz at 16 MHz CPU.
 * High byte (D15-D8) sent first → IC1; low byte (D7-D0) → IC2.
 * VOUT range: 0–5V (= 0x0000–0xFFFF).
 */

#define DAC16_LATCH_BIT  PB3

void dac16_init(void) {
    /* PB4 (/SS) must be output or held high; if left as input and pulled low
     * the ATmega SPI hardware silently switches to slave mode. */
    DDRB |= (1 << DAC16_LATCH_BIT) | (1 << PB4) | (1 << PB5) | (1 << PB7);
    PORTB &= ~(1 << DAC16_LATCH_BIT);
    /* SPE=1, MSTR=1, DORD=0 (MSB first), CPOL=0, CPHA=0, fosc/4 */
    SPCR = (1 << SPE) | (1 << MSTR);
}

void dac16_write(uint16_t val) {
    SPDR = (uint8_t)(val >> 8);           /* high byte first → IC1 */
    while (!(SPSR & (1 << SPIF)));
    SPDR = (uint8_t)(val & 0xFF);         /* low byte → IC2 */
    while (!(SPSR & (1 << SPIF)));
    PORTB |=  (1 << DAC16_LATCH_BIT);    /* latch both 595s simultaneously */
    PORTB &= ~(1 << DAC16_LATCH_BIT);
}
