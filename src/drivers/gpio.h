#pragma once

#include <avr/io.h>
#include <stdint.h>

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
static const GPIO SEG_DP = { &DDRB, &PORTB, &PINB, PB1 }; // 0, dp
static const GPIO SEG_G  = { &DDRB, &PORTB, &PINB, PB2 }; // 1, g
static const GPIO SEG_F  = { &DDRB, &PORTB, &PINB, PB3 }; // 2, f
static const GPIO SEG_E  = { &DDRB, &PORTB, &PINB, PB4 }; // 3, e
static const GPIO SEG_D  = { &DDRB, &PORTB, &PINB, PB5 }; // 4, d
static const GPIO SEG_C  = { &DDRC, &PORTC, &PINC, PC0 }; // 5, c
static const GPIO SEG_B  = { &DDRC, &PORTC, &PINC, PC1 }; // 6, b
static const GPIO SEG_A  = { &DDRC, &PORTC, &PINC, PC2 }; // 7, a

static const GPIO DIG_1  = { &DDRB, &PORTB, &PINB, PB0 }; // D1
static const GPIO DIG_2  = { &DDRD, &PORTD, &PIND, PD7 }; // D2
static const GPIO DIG_3  = { &DDRD, &PORTD, &PIND, PD6 }; // D3
static const GPIO DIG_4  = { &DDRD, &PORTD, &PIND, PD5 }; // D4
static const GPIO GLED   = { &DDRB, &PORTB, &PINB, PB6 }; // GLED
static const GPIO GND    = { &DDRB, &PORTB, &PINB, PB7 }; // GND

// ── pin arrays ─────────────────────────────────────────────────────
static const GPIO SEG_PINS[] = { SEG_DP, SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G };
static const GPIO DIG_PINS[] = { DIG_1, DIG_2, DIG_3, DIG_4 };

// ── helpers ────────────────────────────────────────────────────────
void gpio_set_all_output(void) {
    for (uint8_t i = 0; i < 8; i++) { gpio_output(SEG_PINS[i]); }
    for (uint8_t i = 0; i < 4; i++) { gpio_output(DIG_PINS[i]); }
    gpio_init(GLED, OUTPUT, LOW);
    gpio_init(GND, OUTPUT, LOW);
}

void gpio_set_write(void) {
    for (uint8_t i = 0; i < 8; i++) { gpio_output(SEG_PINS[i]); }
}

void gpio_set_read(void) {
    for (uint8_t i = 0; i < 8; i++) { gpio_input(SEG_PINS[i]); }
}
