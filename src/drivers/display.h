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
