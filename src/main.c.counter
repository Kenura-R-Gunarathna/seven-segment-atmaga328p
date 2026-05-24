#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "utils.h"

int main(void) {
    gpio_set_all_output();
    millis_init();
    sei();

    uint32_t last_count = 0;

    for (;;) {
        display_refresh();

        uint32_t now = millis();

        if (now - last_count >= 1000) {   // increment every second
            last_count += 1000;
            counter_tick();
        }
    }
}
