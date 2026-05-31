// main.c — 0..9999 counter on the 595-driven 7-seg display.
#include <avr/interrupt.h>
#include <avr/io.h>
#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"

#define STEP_MS 100      // increment every 100 ms

int main(void) {
    gpio_set_all_output();   // calls spi595_init()
    millis_init();
    sei();

    uint16_t count = 0;
    uint32_t last  = millis();

    display_write(count);

    for (;;) {
        display_refresh();              // non-blocking multiplex — call every loop

        uint32_t now = millis();
        if (now - last >= STEP_MS) {
            last += STEP_MS;
            count = (count + 1) % 10000; // 0..9999 then wrap
            display_write(count);
        }
    }
    return 0;
}
