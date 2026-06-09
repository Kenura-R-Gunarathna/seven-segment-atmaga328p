/**
 * @file    wavetable.h
 * @brief   Pre-computed waveform lookup table stored in program memory.
 *
 * Contains a 256-entry, 8-bit sine table. Triangle and sawtooth are
 * computed on-the-fly in dds.h (no table needed).
 *
 * Sine table formula
 * ------------------
 *   value[i] = round(127.5 + 127.5 * sin(2 * pi * i / 256))
 *
 * Properties
 * ----------
 *   Entries  : 256
 *   Range    : 0 – 255
 *   Midpoint : 128  (at index 0 and 128)
 *   Peak     : 255  (at index 64)
 *   Trough   :   0  (at index 192)
 *   Storage  : PROGMEM (flash), zero SRAM cost
 */

#pragma once

#include <avr/pgmspace.h>
#include <stdint.h>

/**
 * @brief 256-entry 8-bit sine lookup table in program memory.
 *
 * Index into this table using the top 8 bits of the DDS phase
 * accumulator. Use pgm_read_byte() to read individual entries.
 *
 * Example:
 *   uint8_t sample = pgm_read_byte(&sine_table[idx]);
 */
const uint8_t PROGMEM sine_table[256] = {
    /* idx   0 -  15 */  128,131,134,137,140,143,146,149, 152,156,159,162,165,168,171,174,
    /* idx  16 -  31 */  177,179,182,185,188,190,193,196, 198,201,203,206,208,211,213,215,
    /* idx  32 -  47 */  218,220,222,224,226,228,230,232, 234,235,237,239,240,241,243,244,
    /* idx  48 -  63 */  245,247,248,249,250,250,251,252, 253,253,254,254,254,255,255,255,
    /* idx  64 -  79 */  255,255,255,255,254,254,254,253, 253,252,251,250,250,249,248,247,
    /* idx  80 -  95 */  245,244,243,241,240,239,237,235, 234,232,230,228,226,224,222,220,
    /* idx  96 - 111 */  218,215,213,211,208,206,203,201, 198,196,193,190,188,185,182,179,
    /* idx 112 - 127 */  177,174,171,168,165,162,159,156, 152,149,146,143,140,137,134,131,
    /* idx 128 - 143 */  128,124,121,118,115,112,109,106, 103, 99, 96, 93, 90, 87, 84, 81,
    /* idx 144 - 159 */   78, 76, 73, 70, 67, 65, 62, 59,  57, 54, 52, 49, 47, 44, 42, 40,
    /* idx 160 - 175 */   37, 35, 33, 31, 29, 27, 25, 23,  21, 20, 18, 16, 15, 14, 12, 11,
    /* idx 176 - 191 */   10,  8,  7,  6,  5,  5,  4,  3,   2,  2,  1,  1,  1,  0,  0,  0,
    /* idx 192 - 207 */    0,  0,  0,  0,  1,  1,  1,  2,   2,  3,  4,  5,  5,  6,  7,  8,
    /* idx 208 - 223 */   10, 11, 12, 14, 15, 16, 18, 20,  21, 23, 25, 27, 29, 31, 33, 35,
    /* idx 224 - 239 */   37, 40, 42, 44, 47, 49, 52, 54,  57, 59, 62, 65, 67, 70, 73, 76,
    /* idx 240 - 255 */   78, 81, 84, 87, 90, 93, 96, 99, 103,106,109,112,115,118,121,124
};
