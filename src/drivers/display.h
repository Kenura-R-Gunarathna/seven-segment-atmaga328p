#pragma once

#include "seg.h"
#include "digit.h"
#include "charset.h"
#include "millis.h"

// internal buffer — one pattern per digit
static uint8_t display_buf[4] = { 0, 0, 0, 0 };

// ── write API ─────────────────────────────────────────────────────

// write a 4 digit number 0-9999 into buffer
void display_write(uint16_t number) {
    display_buf[0] = charset_get((number / 1000) % 10);  // thousands
    display_buf[1] = charset_get((number / 100)  % 10);  // hundreds
    display_buf[2] = charset_get((number / 10)   % 10);  // tens
    display_buf[3] = charset_get((number / 1)    % 10);  // units
}

// write a string of up to 4 characters into the buffer
// (e.g., display_write_str("HELP"); or display_write_str("HI"); )
void display_write_str(const char* str) {
    for (uint8_t i = 0; i < 4; i++) {
        if (str[i] != '\0') {
            display_buf[i] = charset_get_char(str[i]);
        } else {
            // If the string is shorter than 4 chars, blank the rest
            display_buf[i] = 0x00;

            // To make shorter strings left-aligned, use the above.
            // If you want them to stop parsing and leave old chars, remove `display_buf[i] = 0x00;`
            return; // stop processing once end of string is reached
        }
    }
}

// write a single character to a specific digit position (0-3)
// (e.g., display_write_char(3, 'C'); for Celsius)
void display_write_char(uint8_t d, char c) {
    if (d < 4) {
        display_buf[d] = charset_get_char(c);
    }
}

// set decimal point on one digit (0-3)
void display_set_dp(uint8_t d) {
    display_buf[d] |= CHARSET_DP;
}

// clear decimal point on one digit (0-3)
void display_clear_dp(uint8_t d) {
    display_buf[d] &= ~CHARSET_DP;
}

// blank entire display
void display_clear(void) {
    for (uint8_t i = 0; i < 4; i++) {
        display_buf[i] = 0x00;
    }
}

// ── refresh — call every loop ─────────────────────────────────────

// Non-blocking multiplexer. Call every loop iteration.
// Switches to next digit every 5ms — full cycle = 20ms = 50Hz refresh.
// Never add delays in the main loop — this handles all timing internally.
void display_refresh(void) {
    static uint8_t  current = 0;
    static uint32_t last    = 0;

    uint32_t now = millis();
    if (now - last < 5) { return; }
    last += 5;

    seg_clear();                       // blank before switching — no ghosting
    digit_deselect(current);           // deactivate current digit
    current = (current + 1) % 4;       // advance to next digit
    seg_set(display_buf[current]);     // load pattern from buffer
    digit_select(current);             // activate next digit
}
