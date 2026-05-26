// main.c — Activity 3: capacitance / transition timer with windowed UI
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/scroll.h"
#include "drivers/comparator.h"

#define WINDOW_READY     0
#define WINDOW_MEASURING 1
#define WINDOW_RESULT    2

#define READY_SCROLL() scroll_start("    MEASURE    ", 300)

volatile uint16_t captured_ticks = 0;
volatile uint8_t  capture_done   = 0;

// Timer1 input capture fires when ACO rises (ACIC routes ACO -> ICP1).
ISR(TIMER1_CAPT_vect) {
    captured_ticks = ICR1;
    TIMSK1 &= ~(1 << ICIE1);          // one-shot: disarm
    capture_done = 1;
}

static void timer1_init(void) {
    TCCR1A = 0;                                          // normal mode
    TCCR1B = (1 << ICES1)                                // capture on rising edge
           | (1 << CS12) | (1 << CS10);                  // ps=1024 -> 1.024 ms/tick @ 1 MHz
    TIMSK1 = 0;                                          // disarmed until measurement starts
}

static void start_measurement(void) {
    TCNT1 = 0;                          // reset counter
    TIFR1 |= (1 << ICF1);               // clear stale capture flag
    capture_done = 0;
    TIMSK1 |= (1 << ICIE1);             // arm capture interrupt
}

int main(void) {
    gpio_set_all_output();
    millis_init();
    sei();

    static const GPIO TOGGLE_SW = { &DDRD, &PORTD, &PIND, PD1 };
    gpio_init(TOGGLE_SW, INPUT, HIGH);  // input + pullup

    comp_enable();
    comp_use_bandgap(1);                // 1.1V on (+)
    comp_use_adc_pin(5);                // PC5 on (-)
    ACSR |= (1 << ACIC);                // route ACO -> Timer1 input capture

    uint32_t settle_start = millis();   // bandgap settling
    while (millis() - settle_start < 50) { }

    timer1_init();

    uint8_t  current_window = WINDOW_READY;
    uint8_t  last_sw_state  = HIGH;
    uint32_t last_press_time = 0;
    uint32_t result_timer   = 0;

    READY_SCROLL();

    for (;;) {
        display_refresh();
        scroll_tick();

        uint32_t now = millis();
        uint8_t  sw  = gpio_read(TOGGLE_SW);
        uint8_t  just_pressed = 0;

        // edge detect + 200ms debounce
        if (sw == LOW && last_sw_state == HIGH) {
            if (now - last_press_time > 200) {
                just_pressed = 1;
                last_press_time = now;
            }
        }
        last_sw_state = sw;

        switch (current_window) {
            case WINDOW_READY:
                if (just_pressed) {
                    scroll_stop();
                    start_measurement();
                    current_window = WINDOW_MEASURING;
                }
                break;

            case WINDOW_MEASURING: {
                // live counter readout: TCNT1 ticks @ 1.024 ms each
                uint16_t t;
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = TCNT1; }
                uint32_t ms = (uint32_t)t * 1024UL / 1000UL;

                if (ms < 10000) {
                    // X.XXX  -> 0.000 to 9.999 s, 1 ms resolution
                    display_write((uint16_t)ms);
                    display_set_dp(0);
                } else {
                    // XX.XX  -> 10.00 to 99.99 s, 10 ms resolution
                    uint32_t cs = ms / 10;
                    if (cs > 9999) cs = 9999;
                    display_write((uint16_t)cs);
                    display_set_dp(1);
                }

                if (capture_done) {
                    // ICR1 ticks * 1024 us/tick = us; / 1000 = ms
                    uint32_t total_ms = (uint32_t)captured_ticks * 1024UL / 1000UL;
                    if (total_ms < 1000) {
                        scroll_start_num(total_ms, 0, 0, " ms", 300);
                    } else {
                        scroll_start_num(total_ms / 1000, total_ms % 1000, 3, " s", 300);
                    }
                    result_timer = now;
                    current_window = WINDOW_RESULT;
                }
                break;
            }

            case WINDOW_RESULT:
                // guard: must view result for 1s before next press counts
                if (just_pressed && (now - result_timer > 1000)) {
                    current_window = WINDOW_READY;
                    READY_SCROLL();
                }
                break;
        }
    }
    return 0;
}
