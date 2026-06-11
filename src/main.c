/*
 * HC-05 Bluetooth test -- sends "Hello World" every second.
 */

#include <avr/io.h>
#include <util/delay.h>

#include "drivers/uart.h"

int main(void) {
    uart_init();

    for (;;) {
        uart_puts("Hello World\r\n");
        _delay_ms(1000);
    }
}
