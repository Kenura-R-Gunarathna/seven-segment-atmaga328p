#pragma once

#include "gpio.h"

// Digit select pins — common anode/cathode control
// HIGH = digit active (adjust if your display is common cathode)
static const GPIO* DIG_MAP[] = {
    &DIG_1,   // index 0
    &DIG_2,   // index 1
    &DIG_3,   // index 2
    &DIG_4,   // index 3
};

// select one, deselect all others in one loop (data route pin)
void digit_select_only(uint8_t d) {
    for (uint8_t i = 0; i < 4; i++) {
        gpio_write(*DIG_MAP[i], i == d ? HIGH : LOW);
    }
}

// just activate one data route pin
void digit_select(uint8_t d)   { gpio_high(*DIG_MAP[d]); }

// just deactivate one data route pin
void digit_deselect(uint8_t d) { gpio_low(*DIG_MAP[d]);  }

// deactivate all data route pins
void digit_deselect_all(void) {
    for (uint8_t i = 0; i < 4; i++) {
        gpio_low(*DIG_MAP[i]);
    }
}
