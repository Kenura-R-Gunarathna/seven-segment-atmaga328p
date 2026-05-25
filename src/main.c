#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/scroll.h"
#include "utils.h"

int main(void) {
    gpio_set_all_output();
    millis_init();
    sei();

    clock_init(21, 13);      // start at 12.12

    uint32_t last_dot  = 0;   // 1Hz  — dot blink
    uint32_t last_tick = 0;   // 1min — clock advance

    // Start the scroll animation
    scroll_start("    READY    ", 300);

    for (;;) {
        display_refresh();

        // uint32_t now = millis();

        // // dot blinks every second
        // if (now - last_dot >= 1000) {
        //     last_dot += 1000;
        //     clock_dot_toggle();   // clean single call
        // }

        // clock advances every minute
        // if (now - last_tick >= 60000) {
        //     last_tick += 60000;
        //     clock_tick();
        // }

        // READY
        // display_write_char(0, 'a');
        // display_write_char(1, 'e');
        // display_write_char(2, 'D');
        // display_write_char(3, 'y');

        // SET
        // display_write_char(0, 's');
        // display_write_char(1, 'e');
        // display_write_char(2, 't');
        // display_write_char(3, ' ');

        // PRESS
        // display_write_char(0, 'P');
        // display_write_char(1, 'A');
        // display_write_char(2, 'e');
        // display_write_char(3, 's');

        scroll_tick();
    }
    return 0;
}
