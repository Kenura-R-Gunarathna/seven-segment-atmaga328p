#pragma once

#include <stddef.h>
#include "millis.h"
#include "display.h"

static struct {
    const char* text;
    uint16_t len;
    uint16_t offset;
    uint16_t speed;
    uint32_t last_time;
    uint8_t  active;    // boolean to check if scrolling is running
} scroll = { NULL, 0, 0, 300, 0, 0 };

// Start the scrolling text
static inline void scroll_start(const char* str, uint16_t speed_ms) {
    scroll.text = str;
    scroll.speed = speed_ms;
    scroll.offset = 0;
    scroll.last_time = millis();
    scroll.active = 1;

    scroll.len = 0;
    while (str[scroll.len] != '\0') { // calculate the length of the scroll text/string
        scroll.len++;
    }
}

// Write val into buf zero-padded to min_digits. Returns chars written.
static inline uint8_t _scroll_write_uint(char* buf, uint32_t val, uint8_t min_digits) {
    char tmp[10];
    uint8_t len = 0;
    do {
        tmp[len++] = '0' + (uint8_t)(val % 10); // digits in lowerst to highest position.
        val /= 10;
    } while (val > 0);
    while (len < min_digits) tmp[len++] = '0'; // prepare the block of digits
    for (uint8_t i = 0; i < len; i++)
        buf[i] = tmp[len - 1 - i]; // digits in highest to lowest position.
    return len;
}

// Scroll a number with units. Always 4 total digits (integer + frac).
// e.g. scroll_start_num(1, 234, 3, " s", 300) -> "  1.234 s  "
// e.g. scroll_start_num(450, 0, 0, " ms", 300) -> "  0450 ms  "
static inline void scroll_start_num(uint32_t integer_part, uint32_t frac_part,
                                     uint8_t frac_digits, const char* unit,
                                     uint16_t speed_ms) {
    static char scroll_num_buf[24];
    uint8_t pos = 0;
    uint8_t int_digits = 4 - frac_digits;

    scroll_num_buf[pos++] = ' ';
    scroll_num_buf[pos++] = ' ';
    pos += _scroll_write_uint(scroll_num_buf + pos, integer_part, int_digits);
    if (frac_digits > 0) {
        scroll_num_buf[pos++] = '.';
        pos += _scroll_write_uint(scroll_num_buf + pos, frac_part, frac_digits);
    }
    while (*unit) scroll_num_buf[pos++] = *unit++;
    scroll_num_buf[pos++] = ' ';
    scroll_num_buf[pos++] = ' ';
    scroll_num_buf[pos]   = '\0';

    scroll_start(scroll_num_buf, speed_ms);
}

// Stop scrolling
static inline void scroll_stop(void) {
    scroll.active = 0;
    display_clear();
}

// Call this every loop. It handles the animation timing.
static inline void scroll_tick(void) {
    if (!scroll.active || scroll.text == NULL) return;

    uint32_t now = millis();
    if (now - scroll.last_time >= scroll.speed) {
        scroll.last_time = now;

        // Write the next 4 characters to the display
        for (uint8_t i = 0; i < 4; i++) {
            if ((scroll.offset + i) < scroll.len) {
                display_write_char(i, scroll.text[scroll.offset + i]);
            } else {
                display_write_char(i, ' '); // blank
            }
        }

        scroll.offset++;

        // Loop back to start
        if (scroll.offset >= scroll.len - 3) { // upon last 4 characters reached reset to start
            scroll.offset = 0;
        }
    }
}
