// main.c — Auto-ranging capacitance meter.
// Hardware: ATmega328P @ 16 MHz crystal.
// See CAP_METER.md for full wiring.

#include <avr/io.h>
#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/scroll.h"
#include "drivers/uart.h"
#include "drivers/hc06.h"
#include "drivers/mux4067.h"
#include "drivers/capmeas.h"

static const GPIO BUTTON = { &DDRD, &PORTD, &PIND, PD2 };

int main(void) {
    gpio_set_all_output();   // SPI/595 display chain
    millis_init();
    uart_init();             // HC-06 UART (PD0/PD1, 9600 8N1)
    mux4067_init();          // CD4067 select lines (PC0-PC2, PD7)
    capmeas_init();          // Timer1 input-capture + comparator
    sei();

    gpio_init(BUTTON, INPUT, HIGH);  // push button (PD2, active-low)
    mux_select(g_range);

    hc06_ready();
    scroll_start("    MEASURE    ", 300);
    uint8_t last_sw = HIGH;

    for (;;) {
        display_refresh();
        scroll_tick();

        uint8_t  sw  = gpio_read(BUTTON);
        int16_t  rx  = hc06_getc();
        uint8_t  go  = (sw == LOW && last_sw == HIGH)
                    || hc06_is_trigger(rx);
        last_sw = sw;

        if (go) {
            capmeas_run();
            while (gpio_read(BUTTON) == LOW) { display_refresh(); }
        }
    }
    return 0;
}
