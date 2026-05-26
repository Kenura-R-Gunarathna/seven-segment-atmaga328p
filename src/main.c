#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/comparator.h"

volatile uint8_t sensor_high = 0;

// Fires on every ACO transition. Latch current state so main loop sees steady value.
ISR(ANALOG_COMP_vect) {
    sensor_high = (comp_read() == 0);  // ACO=0 -> sensor > 1.1V
}

int main(void) {
    gpio_set_all_output();
    millis_init();

    sei();                                      // start millis ticking before any wait

    comp_enable();
    comp_use_bandgap(1);                       // 1.1V on (+)
    comp_use_adc_pin(5);                       // PC5 on (-)

    uint32_t settle_start = millis();          // bandgap + mux settling
    while (millis() - settle_start < 50) { }

    sensor_high = (comp_read() == 0);          // seed initial state, no edge needed
    comp_interrupt_enable(COMP_TRIGGER_TOGGLE); // arm ISR last (clears ACI, enables ACIE)

    for (;;) {
        display_refresh();
        display_write_str(sensor_high ? "HIGH" : "LOW");
    }
    return 0;
}
