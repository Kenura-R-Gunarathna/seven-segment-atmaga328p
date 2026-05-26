#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/scroll.h"
#include "drivers/comparator.h"

// We can use a volatile variable so the ISR can talk to the main loop
volatile uint8_t comparator_triggered = 0;

// This is the Interrupt Service Routine.
// The hardware calls this function INSTANTLY when the comparator triggers!
ISR(ANALOG_COMP_vect) {
    comparator_triggered = 1;
}

int main(void) {
    gpio_set_all_output();
    // millis_init();
    // Set your pins as inputs

    // Setup the Comparator
    comp_enable();

    // Tell comparator to use Internal 1.1V (Frees up PD6!)
    comp_use_bandgap(1);

    // Tell the comparator to use PC5 instead of PD7!
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
        // scroll_tick();

        // uint32_t now = millis();
        // uint8_t current_sw_state = gpio_read(TOGGLE_SW);

        // Check if the hardware interrupt caught a trigger
        if (comparator_triggered) {

            // 1. Reset the flag immediately
            comparator_triggered = 0;

            // 2. Do your reaction timer logic here!
            // current_window = WINDOW_MEASURING;
            gpio_write(BUZZER, HIGH);
            display_write_str("HIGH");
        } else{

            gpio_write(BUZZER, LOW);
            display_write_str("LOW");
        }

    }
    return 0;
}
