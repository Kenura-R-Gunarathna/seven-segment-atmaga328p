/**
 * @file    dds.h
 * @brief   Direct Digital Synthesis (DDS) engine for the ATmega32A.
 *
 * Overview
 * --------
 * Timer2 fires a CTC interrupt at DDS_SAMPLE_RATE (40 kHz). Each interrupt
 * advances a 32-bit phase accumulator and writes the next waveform sample
 * to PORTC, which drives the DAC0808 (U11) directly.
 *
 * Pin / DAC wiring
 * ----------------
 *   PC0 -> DAC A1 (MSB)
 *   PC1 -> DAC A2
 *   ...
 *   PC7 -> DAC A8 (LSB)
 *
 * Because A1 (MSB) lands on the lowest port bit (PC0), every sample byte
 * must be bit-reversed before writing to PORTC. This is handled internally
 * by _bitrev8() inside the ISR.
 *
 * Phase accumulator
 * -----------------
 *   phase_inc = (freq_hz * 2^32) / SAMPLE_RATE
 *   table_idx = phase_acc >> 24   (top 8 bits -> 256-entry table index)
 *
 * Waveforms
 * ---------
 *   0 = sine      (256-entry PROGMEM lookup table)
 *   1 = square    (MSB of phase accumulator -> full on / full off)
 *   2 = triangle  (computed: rises 0->255, falls 255->0)
 *   3 = sawtooth  (computed: 0->255 linear ramp)
 *
 * Timer2 config (ATmega32A, 16 MHz)
 * ----------------------------------
 *   Mode      : CTC
 *   Prescaler : /8
 *   OCR2      : 49
 *   ISR rate  : 16 000 000 / (8 * 50) = 40 000 Hz
 */

#pragma once

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <stdint.h>
#include "wavetable.h"

/** Sample rate in Hz. Determines the Timer2 ISR frequency. */
#define DDS_SAMPLE_RATE 40000UL

/* ── internal state (volatile: shared with ISR) ─────────────────── */

/** 32-bit phase accumulator. Top 8 bits = table index. */
volatile uint32_t _dds_phase_acc = 0;

/** Phase increment per sample. Set by dds_set_freq(). */
volatile uint32_t _dds_phase_inc = 0;

/** Active waveform (0=sine 1=square 2=triangle 3=sawtooth). */
volatile uint8_t  _dds_wave      = 0;

/** Last frequency set by dds_set_freq() or dds_set_freq_raw(). */
volatile uint32_t _dds_freq_hz   = 0;

/* ── private helpers ─────────────────────────────────────────────── */

/**
 * @brief Reverse all 8 bits of a byte (bit 7 <-> bit 0, etc.).
 *
 * Required because DAC0808 A1 (MSB) is wired to PC0 (LSB of PORTC).
 * Three XOR-shift passes complete the reversal in ~12 instructions.
 *
 * @param v  Input byte.
 * @return   Bit-reversed byte.
 */
static inline uint8_t _bitrev8(uint8_t v) {
    v = (uint8_t)(((v >> 4) & 0x0F) | ((v << 4) & 0xF0)); /* swap nibbles    */
    v = (uint8_t)(((v >> 2) & 0x33) | ((v << 2) & 0xCC)); /* swap bit pairs  */
    v = (uint8_t)(((v >> 1) & 0x55) | ((v << 1) & 0xAA)); /* swap odd/even   */
    return v;
}

/* ── ISR ─────────────────────────────────────────────────────────── */

/**
 * @brief Timer2 compare-match ISR — fires at DDS_SAMPLE_RATE (40 kHz).
 *
 * Advances the phase accumulator, computes the next sample for the
 * active waveform, bit-reverses it, then writes it to PORTC.
 */
ISR(TIMER2_COMP_vect) {
    _dds_phase_acc += _dds_phase_inc;
    uint8_t idx = (uint8_t)(_dds_phase_acc >> 24); /* 0-255 table index */

    uint8_t sample;
    switch (_dds_wave) {

        case 1: /* square — MSB of accumulator gives 50 % duty cycle */
            sample = (_dds_phase_acc & 0x80000000UL) ? 255 : 0;
            break;

        case 2: /* triangle — linear rise then fall */
            sample = (idx < 128) ? (uint8_t)(idx << 1)
                                 : (uint8_t)(~(idx << 1));
            break;

        case 3: /* sawtooth — monotone 0->255 ramp */
            sample = idx;
            break;

        default: /* sine — PROGMEM lookup */
            sample = pgm_read_byte(&sine_table[idx]);
            break;
    }

    PORTC = sample; /* PC7=A1(MSB), PC0=A8(LSB) — no bitrev needed */
}

/* ── public API ──────────────────────────────────────────────────── */

/**
 * @brief Initialise the DDS engine.
 *
 * Sets PORTC as output (DAC0808 data bus), configures Timer2 in CTC
 * mode at DDS_SAMPLE_RATE, and enables the compare-match interrupt.
 * Call before sei().
 */
void dds_init(void) {
    DDRC  = 0xFF;  /* all PORTC pins output -> DAC0808 data bus */
    PORTC = 0x00;

    /*
     * Timer2 CTC, prescaler /8:
     *   TCCR2: WGM21=1 (bit 3) | CS21=1 (bit 1)
     *   OCR2 = F_CPU / (8 * SAMPLE_RATE) - 1 = 49 @ 16 MHz / 40 kHz
     */
    TCCR2 = (1 << WGM21) | (1 << CS21);
    OCR2  = (uint8_t)(F_CPU / 8UL / DDS_SAMPLE_RATE - 1);
    TIMSK |= (1 << OCIE2);
}

/**
 * @brief Set the output frequency.
 *
 * Computes a new phase increment from the requested frequency and
 * applies it atomically so the ISR never sees a torn 32-bit value.
 * Also resets the phase accumulator to avoid a glitch on the output.
 *
 * @param hz  Desired frequency in Hz (valid range: 1 – 20 000).
 */
void dds_set_freq(uint32_t hz) {
    _dds_freq_hz = hz;
    uint32_t inc = (uint32_t)((uint64_t)hz * 4294967296ULL / DDS_SAMPLE_RATE);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        _dds_phase_acc = 0;
        _dds_phase_inc = inc;
    }
}

/* Used by mode.h when sample rate differs from DDS_SAMPLE_RATE (e.g. BOTH mode). */
void dds_set_freq_raw(uint32_t hz, uint32_t sample_rate_hz) {
    _dds_freq_hz = hz;
    uint32_t inc = (uint32_t)((uint64_t)hz * 4294967296ULL / sample_rate_hz);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        _dds_phase_acc = 0;
        _dds_phase_inc = inc;
    }
}

/**
 * @brief Select the output waveform.
 *
 * @param wave  0 = sine | 1 = square | 2 = triangle | 3 = sawtooth
 */
void dds_set_wave(uint8_t wave) {
    _dds_wave = wave & 0x03;
}

/* ── bit-bang output (no interrupts) ──────────────────────────────── */

/**
 * @brief Blocking binary up-counter on PORTC. Counts 0x00->0xFF forever.
 *        Probe each pin: PC0 toggles fastest, PC7 slowest (÷128).
 *        Use this to verify all 8 DAC data lines before running the sine.
 * @param step_delay_loops  Busy-wait per count step (raise to slow down).
 */
void dds_portc_bincnt(uint16_t step_delay_loops) {
    DDRC  = 0xFF;
    PORTC = 0x00;
    uint8_t cnt = 0;
    for (;;) {
        PORTC = cnt++;
        for (volatile uint16_t d = 0; d < step_delay_loops; d++) { }
    }
}

/**
 * @brief Blocking bit-banged sine output on PORTC (DAC0808 bus).
 *
 * No Timer2, no interrupts — just walks the 256-entry sine table and
 * writes each (bit-reversed) sample straight to PORTC in a tight loop.
 * This is the simplest possible waveform generator: rock-solid, but it
 * blocks the CPU forever, so nothing else (LEDs, UART) runs meanwhile.
 *
 * Output frequency is set by an inter-sample busy delay:
 *   f_out = F_CPU / (256 * cycles_per_sample)
 * Larger @p step_delay_loops -> lower frequency.
 *
 * @param step_delay_loops  Busy-wait iterations between samples (0 = fastest).
 *                          Each unit ~= 3 CPU cycles.
 */
void dds_sine_bitbang(uint16_t step_delay_loops) {
    DDRC  = 0xFF;   /* PORTC = DAC0808 data bus, all outputs */
    PORTC = 0x00;

    for (;;) {
        for (uint16_t i = 0; i < 256; i++) {
            PORTC = pgm_read_byte(&sine_table[i]); /* PC7=A1(MSB) — no bitrev */
            for (volatile uint16_t d = 0; d < step_delay_loops; d++) { }
        }
    }
}
