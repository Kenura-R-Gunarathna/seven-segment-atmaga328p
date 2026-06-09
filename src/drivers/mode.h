#pragma once

#include <avr/io.h>
#include <stdint.h>
#include "dds.h"

/*
 * Three operating modes:
 *
 *   MODE_FG   (m0) — Function generator only.
 *                     Timer2 @ 40kHz, PORTC 8-bit R-2R, max output 20kHz.
 *                     SAR ADC disabled.
 *
 *   MODE_OSC  (m1) — Oscilloscope only.
 *                     Timer2 stopped, PORTC=0 (DAC output silenced).
 *                     SAR runs freely in main loop at ~6kSPS.
 *
 *   MODE_BOTH (m2) — Both simultaneously.
 *                     Timer2 @ 20kHz, max FG output 10kHz.
 *                     SAR interleaves with ISR calls, ~1kSPS.
 *                     Phase increment recalculated for new sample rate.
 */

#define DDS_RATE_FG    40000UL   /* Timer2 in MODE_FG and default */
#define DDS_RATE_BOTH  20000UL   /* Timer2 in MODE_BOTH */

typedef enum { MODE_FG = 0, MODE_OSC = 1, MODE_BOTH = 2 } op_mode_t;

static op_mode_t _op_mode = MODE_FG;

op_mode_t mode_get(void) { return _op_mode; }

void mode_set(op_mode_t m) {
    _op_mode = m;
    switch (m) {

        case MODE_FG:
            /* Timer2 CTC, /8 prescaler → 40kHz */
            TCCR2 = (1 << WGM21) | (1 << CS21);
            OCR2  = (uint8_t)(F_CPU / 8UL / DDS_RATE_FG - 1);   /* 49 */
            TIMSK |= (1 << OCIE2);
            /* Restore phase_inc for 40kHz sample rate */
            dds_set_freq_raw(_dds_freq_hz, DDS_RATE_FG);
            break;

        case MODE_OSC:
            /* Stop Timer2 — silence function gen DAC */
            TCCR2 = 0;
            TIMSK &= ~(1 << OCIE2);
            PORTC = 0x00;
            break;

        case MODE_BOTH:
            /* Timer2 CTC, /8 prescaler → 20kHz (halved) */
            TCCR2 = (1 << WGM21) | (1 << CS21);
            OCR2  = (uint8_t)(F_CPU / 8UL / DDS_RATE_BOTH - 1); /* 99 */
            TIMSK |= (1 << OCIE2);
            /* Recalculate phase_inc for new (halved) sample rate */
            dds_set_freq_raw(_dds_freq_hz, DDS_RATE_BOTH);
            break;
    }
}
