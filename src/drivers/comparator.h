// comparator.h
#pragma once

#include <avr/io.h>
#include <stdint.h>

// ── Constants for Interrupt Trigger Modes ──────────────────────────
#define COMP_TRIGGER_TOGGLE  0x00  // Interrupt on any edge
#define COMP_TRIGGER_FALLING 0x02  // Interrupt on falling edge
#define COMP_TRIGGER_RISING  0x03  // Interrupt on rising edge

// ── Base Functions ─────────────────────────────────────────────────

// Turn the comparator ON (it is on by default, but good practice)
static inline void comp_enable(void) {
    ACSR &= ~(1 << ACD); // Clear the Disable bit to turn it ON
}

// Turn the comparator OFF (saves power if running on battery)
static inline void comp_disable(void) {
    ACSR |= (1 << ACD);  // Set the Disable bit to turn it OFF
}

// Read the current output of the comparator (0 or 1)
static inline uint8_t comp_read(void) {
    return (ACSR & (1 << ACO)) ? 1 : 0;
}

// ── Advanced Configuration ─────────────────────────────────────────

// Route internal 1.1V bandgap to the POSITIVE (+) input in place of AIN0/PD6.
// Note: bandgap is hardware-locked to (+). Use comp_use_adc_pin() for the (-) side.
static inline void comp_use_bandgap(uint8_t enable) {
    if (enable) {
        ACSR |=  (1 << ACBG);
    } else {
        ACSR &= ~(1 << ACBG);
    }
}

// ── Interrupt Setup ────────────────────────────────────────────────

// Configure and enable the comparator hardware interrupt
static inline void comp_interrupt_enable(uint8_t trigger_mode) {
    // Disable interrupt temporarily while changing settings
    ACSR &= ~(1 << ACIE);

    // Clear the old trigger mode bits (ACIS1 and ACIS0)
    ACSR &= ~((1 << ACIS1) | (1 << ACIS0));

    // Set the new trigger mode
    ACSR |= (trigger_mode << ACIS0);

    // Clear any pending interrupt flags (write 1 to ACI to clear it!)
    ACSR |= (1 << ACI);

    // Re-enable the interrupt
    ACSR |= (1 << ACIE);
}

// Disable the comparator interrupt
static inline void comp_interrupt_disable(void) {
    ACSR &= ~(1 << ACIE);
}

// Route the Negative (-) input to an Analog Pin (PC0 - PC5) instead of PD7
static inline void comp_use_adc_pin(uint8_t adc_channel) {
    // MUST turn off the ADC hardware to borrow its multiplexer!
    ADCSRA &= ~(1 << ADEN);

    // Turn on the bridge between the ADC MUX and the Comparator
    ADCSRB |= (1 << ACME);

    // Clear the old pin selection in the MUX (bits 2, 1, 0)
    ADMUX &= ~0x07;

    // Set the new pin!
    // (e.g., 5 = PC5, 4 = PC4, 3 = PC3)
    ADMUX |= (adc_channel & 0x07);
}
