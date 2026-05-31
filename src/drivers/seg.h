#pragma once

#include "spi595.h"
#include "charset.h"

// Write an 8-bit segment pattern to the segment 595, then latch.
// Bit order matches CHARSET (bit7=a .. bit0=dp); shipped as-is under
// LSB-first SPI so QA=bit7 .. QH=bit0. No timing — caller is responsible
// for hold delay and clearing before switching digits.
void seg_set(uint8_t pattern) {
    sr_seg = pattern;
    sr_flush();
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
