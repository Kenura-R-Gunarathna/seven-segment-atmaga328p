#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/scroll.h"
#include <stdlib.h>

// Define the "Windows" (States)
#define WINDOW_READY     0
#define WINDOW_WAITING   1
#define WINDOW_MEASURING 2
#define WINDOW_RESULT    3

#define READY_SCROLL() scroll_start("    READY    ", 300)

int main(void) {
    gpio_set_all_output();
    millis_init();
    sei();

    static const GPIO TOGGLE_SW = { &DDRD, &PORTD, &PIND, PD1 }; // Toggle switch
    static const GPIO BUZZER    = { &DDRD, &PORTD, &PIND, PD0 }; // Buzzer

    gpio_init(TOGGLE_SW, INPUT, HIGH);
    gpio_init(BUZZER, OUTPUT, LOW);

    // State Machine Variables
    uint8_t  current_window = WINDOW_READY;
    uint32_t wait_timer     = 0;
    uint32_t random_delay   = 0;
    uint32_t start_time     = 0;

    // NEW: Guard timer so the user is forced to look at their score!
    uint32_t result_timer   = 0;

    // Button Edge Detection Variables
    uint8_t  last_sw_state   = HIGH;
    uint32_t last_press_time = 0;

    // Start initial state
    READY_SCROLL();

    for (;;) {
        display_refresh();
        scroll_tick();

        uint32_t now = millis();
        uint8_t current_sw_state = gpio_read(TOGGLE_SW);
        uint8_t just_pressed = 0;

        // --- EDGE DETECTION (Single Clicks Only) ---
        if (current_sw_state == LOW && last_sw_state == HIGH) {
            if (now - last_press_time > 200) {
                just_pressed = 1;
                last_press_time = now;
            }
        }
        last_sw_state = current_sw_state;

        switch (current_window) {
            case WINDOW_READY:
                if (just_pressed) {
                    current_window = WINDOW_WAITING;
                    scroll_stop();
                    display_write_str("----");
                    srand(now);
                    random_delay = (rand() % 3000) + 2000;
                    wait_timer = now;
                }
                break;

            case WINDOW_WAITING:
                if (just_pressed) {
                    current_window = WINDOW_RESULT;
                    scroll_start("    TOO EARLY    ", 300);
                    result_timer = now; // Start the Guard Timer
                }
                else if (now - wait_timer >= random_delay + 3000) {
                    current_window = WINDOW_MEASURING;
                    gpio_write(BUZZER, HIGH);
                    start_time = now;
                    scroll_start("    PRESS    ", 300);
                }
                break;

            case WINDOW_MEASURING:
                // Short beep (100ms)
                if (now - start_time >= 100) {
                    gpio_write(BUZZER, LOW);
                }

                if (just_pressed) {
                    gpio_write(BUZZER, LOW);

                    uint32_t reaction_time = (now - start_time);

                    if (reaction_time >= 60000) {
                        scroll_start("    SLOW    ", 300);
                    } else if (reaction_time >= 10000) {
                        scroll_start_num(reaction_time / 1000, (reaction_time % 1000) / 10, 2, " s", 300);
                    } else if (reaction_time >= 1000) {
                        scroll_start_num(reaction_time / 1000, reaction_time % 1000, 3, " s", 300);
                    } else {
                        scroll_start_num(reaction_time, 0, 0, " ms", 300); // shown as ns
                    }

                    result_timer = now;
                    current_window = WINDOW_RESULT;
                }
                break;

            case WINDOW_RESULT:
                // --- THE BUG FIX ---
                // Only allow the button to restart the game IF 2 seconds (2000ms)
                // have passed since they got their score!
                if (just_pressed && (now - result_timer > 5000)) {
                    current_window = WINDOW_READY;
                    READY_SCROLL();
                }
                break;
        }
    }
    return 0;
}
