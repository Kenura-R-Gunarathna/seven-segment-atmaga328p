#pragma once

#include <stdint.h>

// bit position: dp g f e d c b a
//               7  6 5 4 3 2 1 0

const uint8_t CHARSET[] = {
    0xFC, // 0
    0x60, // 1
    0xDA, // 2
    0xF2, // 3
    0x66, // 4
    0xB6, // 5
    0xBE, // 6
    0xE0, // 7
    0xFE, // 8
    0xF6, // 9
    0xEE, // A
    0x3E, // b
    0x9C, // C
    0x7A, // d
    0x9E, // E
    0x8E, // F
};

#define CHARSET_DP    0x01   // decimal point bit

// returns the character set pattern for the given digit (0-9)
uint8_t charset_get(uint8_t n) {
    if (n > 9) { return 0; }      // safety — blank if out of range
    return CHARSET[n];
}

// returns the character set pattern for the given digit (0-9) with dp on
uint8_t charset_get_dp(uint8_t n) {
    return charset_get(n) | CHARSET_DP;   // same digit with dp on
}
