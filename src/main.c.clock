#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "utils.h"

int main(void) {
    gpio_set_all_output();
    millis_init();
    sei();

    clock_init(12, 12);      // start at 12.12

    uint32_t last_dot  = 0;   // 1Hz  — dot blink
    uint32_t last_tick = 0;   // 1min — clock advance

    for (;;) {
        display_refresh();

        uint32_t now = millis();

        // dot blinks every second
        if (now - last_dot >= 1000) {
            last_dot += 1000;
            clock_dot_toggle();   // clean single call
        }

        // clock advances every minute
        if (now - last_tick >= 60000) {
            last_tick += 60000;
            clock_tick();
        }
    }
    return 0;
}
