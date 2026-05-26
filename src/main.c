// main.c

#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/comparator.h"

int main(void) {
    gpio_set_all_output();
    millis_init();

    // Setup the Comparator
    comp_enable();

    // Set the comparator to use Internal 1.1V (Frees up PD6!)
    comp_use_bandgap(1);

    // Set the comparator to use PC5 instead of PD7!
    comp_use_adc_pin(5);

    // Set it to trigger an interrupt when the sensor voltage drops
    comp_interrupt_enable(COMP_TRIGGER_FALLING);

    sei(); // Enable interrupts

    static const GPIO TOGGLE_SW = { &DDRD, &PORTD, &PIND, PD1 }; // Toggle switch
    static const GPIO BUZZER    = { &DDRD, &PORTD, &PIND, PD0 }; // Buzzer

    gpio_init(TOGGLE_SW, INPUT, HIGH);
    gpio_init(BUZZER, OUTPUT, LOW);

    for (;;) {
        display_refresh();

        if (comp_read()) {
            gpio_write(BUZZER, HIGH);
            display_write_str("HIGH");
        } else{

            gpio_write(BUZZER, LOW);
            display_write_str("LOW");
        }

    }
    return 0;
}
