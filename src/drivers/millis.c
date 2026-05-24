#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>

static volatile uint32_t _ticks_ms = 0;

void millis_init(void) {
  // Timer0, CTC mode, prescaller /8, OCR0A = 124
  // f_isr = 1e6 / 8 / (124 + 1) = 1000 Hz -> 1 ms tick
  TCCR0A = (1 << WGM01);              // CTC
  TCCR0B = (1 << CS01);               // /8
  OCR0A = 124;
  TIMSK0 = (1 << OCIE0A); // enable compare-A interrupt
}

uint32_t millis(void) {
  uint32_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    v = _ticks_ms;
  } // 32-bit read is not atomic on AVR
  return v;
}

ISR(TIMER0_COMPA_vect) { _ticks_ms++; }
