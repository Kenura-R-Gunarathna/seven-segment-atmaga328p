#pragma once

#include "gpio.h"
#include "charset.h"

// segment bit positions matching CHARSET bit order
// bit: 0   1   2   3   4   5   6   7
// seg: dp  g   f   e   d   c   b   a
// our assumed order        // your actual order
static const GPIO* SEG_MAP[] = {
    &SEG_DP,  // bit 0     // &SEG_DP  bit 7
    &SEG_G,   // bit 1     // &SEG_G   bit 6
    &SEG_F,   // bit 2     // &SEG_F   bit 5
    &SEG_E,   // bit 3     // &SEG_E   bit 4
    &SEG_D,   // bit 4     // &SEG_D   bit 3
    &SEG_C,   // bit 5     // &SEG_C   bit 2
    &SEG_B,   // bit 6     // &SEG_B   bit 1
    &SEG_A,   // bit 7     // &SEG_A   bit 0
};

// Write an 8-bit segment pattern to physical pins instantly.
// Bit 0 = SEG_A ... Bit 7 = SEG_DP. No timing — caller is responsible
// for hold delay and clearing before switching digits.
void seg_set(uint8_t pattern) {
    for (uint8_t i = 0; i < 8; i++) {
        gpio_write(*SEG_MAP[i], (pattern >> i) & 1);
    }
}

void seg_clear(void) { seg_set(0x00); }   // all segments off
void seg_all(void)   { seg_set(0xFF); }   // all segments on

// write a digit 0-9 to segments
void seg_digit(uint8_t n) {
    seg_set(charset_get(n));
}

// write a digit with decimal point
void seg_digit_dp(uint8_t n) {
    seg_set(charset_get_dp(n));
}
