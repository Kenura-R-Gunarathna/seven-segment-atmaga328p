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
