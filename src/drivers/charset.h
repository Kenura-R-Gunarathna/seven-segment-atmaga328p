#pragma once

#include <stdint.h>

// bit position: dp g f e d c b a
//               7  6 5 4 3 2 1 0

const uint8_t CHARSET[] = {
    // --- Numbers 0-9 (Indexes 0-9) ---
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

    // --- Alphabet A-Z (Indexes 10-35) ---
    0xEE, // A  (10)
    0x3E, // b  (11) - lowercase
    0x9C, // C  (12)
    0x7A, // d  (13) - lowercase
    0x9E, // E  (14)
    0x8E, // F  (15)
    0xBC, // G  (16)
    0x6E, // H  (17)
    0x60, // I  (18) - Same as 1
    0x78, // J  (19)
    0xAE, // K  (20) - Approximation
    0x1C, // L  (21)
    0xEC, // M  (22) - Pi shape approximation
    0x2A, // n  (23) - lowercase
    0xFC, // O  (24) - Same as 0
    0xCE, // P  (25)
    0xF6, // Q  (26) - Same as 9
    0x0A, // r  (27) - lowercase
    0xB6, // S  (28) - Same as 5
    0x1E, // t  (29) - lowercase
    0x7C, // U  (30)
    0x38, // v  (31) - lowercase (c,d,e)
    0x7C, // W  (32) - Same as U
    0x6E, // X  (33) - Same as H
    0x76, // Y  (34)
    0xDA  // Z  (35) - Same as 2
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

// Helper function: Convert an ASCII char to its 7-segment pattern
uint8_t charset_get_char(char c) {
    // 1. Handle Numbers '0' through '9'
    if (c >= '0' && c <= '9') {
        return CHARSET[c - '0'];
    }

    // 2. Handle Dash / Hyphen explicitly (Only segment g is ON)
    if (c == '-') {
        return 0x02;
    }

    // 2b. Handle decimal point as a standalone glyph
    if (c == '.') {
        return CHARSET_DP;
    }

    // 3. Convert lowercase to uppercase automatically
    if (c >= 'a' && c <= 'z') {
        c -= 32;
    }

    // 4. Handle Letters 'A' through 'Z'
    if (c >= 'A' && c <= 'Z') {
        return CHARSET[c - 'A' + 10]; // Mathematically maps 'A' to index 10, 'B' to 11, etc.
    }

    // 5. Return Blank (0x00) for spaces or unsupported symbols
    return 0x00;
}
