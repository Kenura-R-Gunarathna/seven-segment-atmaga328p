#pragma once

#include "spi595.h"

// Digit select — drives the digit-transistor 595.
// Confirmed empirically: D1=bit5 (0x20), D4=bit2 (0x04). Linear run:
//   D1=bit5, D2=bit4, D3=bit3, D4=bit2.
// If a digit lands in the wrong spot, just edit this table.
static const uint8_t DIG_BIT[4] = { 2, 3, 4, 5 };   // index 0..3 -> physical L..R

// select one, deselect all others
void digit_select_only(uint8_t d) {
    sr_dig = (1 << DIG_BIT[d]);
    sr_flush();
}

// activate one digit
void digit_select(uint8_t d) {
    sr_dig |= (1 << DIG_BIT[d]);
    sr_flush();
}

// deactivate one digit
void digit_deselect(uint8_t d) {
    sr_dig &= ~(1 << DIG_BIT[d]);
    sr_flush();
}

// deactivate all digits
void digit_deselect_all(void) {
    sr_dig = 0;
    sr_flush();
}
