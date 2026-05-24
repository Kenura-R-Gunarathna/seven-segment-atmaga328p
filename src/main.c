#include <avr/io.h>
#include <util/delay.h>
#include "drivers/millis.h"

int main(void) {
    DDRB |= (1 << PB5);
    millis_init();
    
    while(1) {
        PORTB ^= (1 << PB5);
        _delay_ms(500);
    }
    return 0;
}
