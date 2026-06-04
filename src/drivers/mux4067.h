#pragma once

#include <avr/io.h>
#include <stdint.h>
#include "gpio.h"

// ── CD4067 16-ch analog mux — range-resistor selection ──────────────
// S0..S2 = PC0..PC2, S3 = PD7, INH -> GND (always enabled).
// MUX_R[] holds EFFECTIVE resistances (R_nom + R_on), where R_on ≈ 150 Ω
// measured from two independent 8 MHz and 16 MHz data points.

// Calibration: C[pF] = dt * CAL_K / R_eff + CAL_B.
// CAL_K = 721500 is the crystal-ideal (t_tick*1e12/ln2 = 0.5e-6*1e12/0.693).
// Fine-tune with 2-point film-cap fit at a mid range: CAL_K_new = a * R_eff.
// CAL_B ≈ 650 pF (stray + comparator offset) — clock-independent.
#define CAL_K       721500UL
#define CAL_B       650L

// Auto-range window (timer ticks). Below DT_MIN -> quantization too coarse;
// above DT_MAX -> 16-bit timer overflows.
#define DT_MIN      200U
#define DT_MAX      58000U
#define N_TARGET    6000U          // aim point for the interpolation jump

// Lock to one channel for bench testing (-1 = auto-ranging enabled).
#define FORCE_RANGE -1

// Effective channel resistances (R_nom + R_on). Index = 4067 channel number.
// Add new resistors in ascending R order; NUM_RANGES updates automatically.
static const uint32_t MUX_R[] = {
    160UL, 206UL, 704UL, 1130UL, 5669UL, 10010UL,
    56520UL, 99450UL, 560150UL, 1050150UL, 5600150UL, 10000150UL
};
#define NUM_RANGES ((uint8_t)(sizeof(MUX_R) / sizeof(MUX_R[0])))

static uint8_t g_range = 7;        // last-used channel (warm start, init mid)

void mux4067_init(void) {
    DDRC |= 0x07;                  // S0..S2 = PC0..PC2 outputs
    DDRD |= (1 << PD7);            // S3 = PD7 output
}

void mux_select(uint8_t ch) {
    PORTC = (PORTC & ~0x07) | (ch & 0x07);
    if (ch & 0x08) { PORTD |=  (1 << PD7); }
    else           { PORTD &= ~(1 << PD7); }
}

// Forward declaration — measure_once() is defined in capmeas.h which
// #includes this file. Declare it here so auto_range() can call it.
static uint16_t measure_once(void);

// Hybrid range search: warm-start probe -> interpolation jump -> verify walk.
// Converges in ~1 measurement for repeated caps, ~2-3 for cold jumps.
static uint8_t auto_range(void) {
    uint8_t  ch = g_range;
    mux_select(ch);
    uint16_t N = measure_once();                 // 1) warm-start probe

    if (N < DT_MIN || N > DT_MAX) {              // miss -> interpolation jump
        uint32_t Nc = (N == 0) ? 100000UL : N;
        uint32_t r_target = (uint32_t)((uint64_t)MUX_R[ch] * N_TARGET / Nc);

        uint8_t j = 0;
        for (uint8_t i = 0; i < NUM_RANGES; i++) {
            if (MUX_R[i] <= r_target) { j = i; }
        }
        ch = j;

        for (uint8_t k = 0; k < NUM_RANGES; k++) {   // 3) bounded verify-walk
            mux_select(ch);
            N = measure_once();
            if (N == 0 || N > DT_MAX) { if (ch > 0) ch--; else break; }
            else if (N < DT_MIN)      { if (ch < NUM_RANGES - 1) ch++; else break; }
            else break;
        }
    }
    g_range = ch;
    return ch;
}
