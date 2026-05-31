#pragma once

#include "drivers/display.h"

// ── counter ───────────────────────────────────────────────────────

static uint16_t _counter = 0;

void counter_init(uint16_t start) {
    _counter = start;
    display_write(_counter);
}

void counter_tick(void) {
    if (_counter < 9999) {
        _counter++;
    } else {
        _counter = 0;        // rollover
    }
    display_write(_counter);
}

void counter_reset(void) {
    _counter = 0;
    display_write(_counter);
}

uint16_t counter_get(void) {
    return _counter;
}

// ── clock — format 00.00 (HH.MM) ─────────────────────────────────

static uint8_t _dot = 0;   // 0 = off, 1 = on
static uint8_t _clock_hh = 0;   // hours   0-23
static uint8_t _clock_mm = 0;   // minutes 0-59
static uint8_t _clock_ss = 0;   // seconds 0-59 (not shown on 4-digit, sent over BT)

// Write MM.SS to display buffer
void clock_refresh(void) {
    display_buf[0] = CHARSET[_clock_hh / 10];                // tens of hours
    display_buf[1] = CHARSET[_clock_hh % 10] | (_dot ? CHARSET_DP : 0);
    display_buf[2] = CHARSET[_clock_mm / 10];                // tens of minutes
    display_buf[3] = CHARSET[_clock_mm % 10];                // units of minutes
}

// set the full time (seconds included)
void clock_set(uint8_t hh, uint8_t mm, uint8_t ss) {
    _clock_hh = hh;
    _clock_mm = mm;
    _clock_ss = ss;
    clock_refresh();
}

void clock_init(uint8_t hh, uint8_t mm) {
    clock_set(hh, mm, 0);
}

void clock_reset(void) {
    clock_init(0, 0);
}

void clock_dot_toggle(void) {
    _dot ^= 1;
    clock_refresh();   // immediately updates buffer
}

// read current time (for logging / Bluetooth)
uint8_t clock_get_hh(void) { return _clock_hh; }
uint8_t clock_get_mm(void) { return _clock_mm; }
uint8_t clock_get_ss(void) { return _clock_ss; }

// advance one second, cascading into minutes/hours (call every 1s)
void clock_tick_sec(void) {
    _clock_ss++;
    if (_clock_ss >= 60) {
        _clock_ss = 0;
        _clock_mm++;
        if (_clock_mm >= 60) {
            _clock_mm = 0;
            _clock_hh++;
            if (_clock_hh >= 24) { _clock_hh = 0; }
        }
    }
    clock_refresh();
}

// call every minute
void clock_tick(void) {
    _clock_mm++;
    if (_clock_mm >= 60) {
        _clock_mm = 0;
        _clock_hh++;
        if (_clock_hh >= 24) {
            _clock_hh = 0;   // rollover at midnight
        }
    }
    clock_refresh();
}
