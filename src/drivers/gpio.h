#pragma once

#include <avr/io.h>
#include <stdint.h>
#include "spi595.h"

// ── types ──────────────────────────────────────────────────────────
typedef struct {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pin;
    uint8_t           bit;
} GPIO;

// ── constants ──────────────────────────────────────────────────────
#define HIGH   1
#define LOW    0
#define OUTPUT 1
#define INPUT  0

// ── base functions ─────────────────────────────────────────────────
void gpio_mode(GPIO g, uint8_t mode) {
    if (mode) *g.ddr |=  (1 << g.bit);
    else      *g.ddr &= ~(1 << g.bit);
}
void gpio_write(GPIO g, uint8_t v) {
    if (v) *g.port |=  (1 << g.bit);
    else   *g.port &= ~(1 << g.bit);
}
uint8_t gpio_read(GPIO g) { return (*g.pin >> g.bit) & 1; }
void    gpio_tog(GPIO g)  { *g.port ^= (1 << g.bit); }

// ── shorthand macros ───────────────────────────────────────────────
#define gpio_output(g)          gpio_mode(g, OUTPUT)
#define gpio_input(g)           gpio_mode(g, INPUT)
#define gpio_high(g)            gpio_write(g, HIGH)
#define gpio_low(g)             gpio_write(g, LOW)
#define gpio_init(g, mode, val) do { gpio_mode(g, mode); gpio_write(g, val); } while(0)

// ── pin definitions ────────────────────────────────────────────────
// ATmega32A: XTAL1/XTAL2 are on dedicated pins 12/13, not port pins.
// PB5=MOSI, PB7=SCK are used by SPI — do not use as general GPIO.

// ── helpers ────────────────────────────────────────────────────────
void gpio_set_all_output(void) {
    spi595_init();                  // segments + digits via SPI shift registers
}
