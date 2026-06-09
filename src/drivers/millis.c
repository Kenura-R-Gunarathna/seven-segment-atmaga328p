/**
 * @file    millis.c
 * @brief   Millisecond tick counter using Timer0 CTC on ATmega32A.
 *
 * Timer0 is configured in CTC (Clear Timer on Compare) mode.
 * The compare-match ISR increments a 32-bit counter every 1 ms.
 *
 * Timer0 config (ATmega32A, 16 MHz)
 * -----------------------------------
 *   Mode      : CTC  (WGM01 = 1)
 *   Prescaler : /64  (CS01 | CS00)
 *   OCR0      : 249
 *   ISR rate  : 16 000 000 / (64 * 250) = 1 000 Hz  -> 1 ms tick
 *
 * Note: ATmega32A uses a single TCCR0 register (not TCCR0A/TCCR0B
 * as on ATmega328P). The interrupt enable bit is OCIE0 in TIMSK
 * and the vector is TIMER0_COMP_vect.
 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>

/** Running millisecond counter. Max value: ~49.7 days before rollover. */
static volatile uint32_t _ticks_ms = 0;

/**
 * @brief Initialise Timer0 for a 1 ms tick.
 *
 * Selects prescaler automatically based on F_CPU so the 8-bit OCR0
 * register is never exceeded. Must be called before sei().
 */
void millis_init(void) {
#if F_CPU <= 2000000UL
    /* Prescaler /8  -> OCR0 = F_CPU/8/1000 - 1  (e.g. 1 MHz -> 124) */
    TCCR0 = (1 << WGM01) | (1 << CS01);
    OCR0  = (uint8_t)(F_CPU / 8UL / 1000UL - 1);
#else
    /* Prescaler /64 -> OCR0 = F_CPU/64/1000 - 1  (16 MHz -> 249)     */
    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
    OCR0  = (uint8_t)(F_CPU / 64UL / 1000UL - 1);
#endif
    TIMSK |= (1 << OCIE0); /* enable Timer0 compare-match interrupt */
}

/**
 * @brief Return the number of milliseconds since millis_init() was called.
 *
 * Uses an atomic block to guarantee a consistent 32-bit read across
 * the four bytes of _ticks_ms (AVR has no atomic 32-bit load).
 *
 * @return  Elapsed time in milliseconds (wraps after ~49.7 days).
 */
uint32_t millis(void) {
    uint32_t v;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        v = _ticks_ms;
    }
    return v;
}

/** @brief Timer0 compare-match ISR — increments the ms counter. */
ISR(TIMER0_COMP_vect) { _ticks_ms++; }
