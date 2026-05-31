#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>

static volatile uint32_t _ticks_ms = 0; // MAX: 4,294,967,295 ms; 2^32-1 counts

void millis_init(void) {
  // Timer0, CTC mode -> 1 ms tick. OCR0A is 8-bit, so pick a prescaler that
  // keeps (F_CPU/presc/1000 - 1) <= 255. Scales with F_CPU automatically.
  TCCR0A = (1 << WGM01);                       // CTC
#if F_CPU <= 2000000UL
  TCCR0B = (1 << CS01);                        // /8   (1 MHz -> OCR0A=124)
  OCR0A  = (uint8_t)(F_CPU / 8UL / 1000UL - 1);
#else
  TCCR0B = (1 << CS01) | (1 << CS00);          // /64  (8MHz->124, 16MHz->249)
  OCR0A  = (uint8_t)(F_CPU / 64UL / 1000UL - 1);
#endif
  TIMSK0 = (1 << OCIE0A);                       // enable compare-A interrupt
}

uint32_t millis(void) {
  uint32_t v;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    v = _ticks_ms;
  } // 32-bit read is not atomic on AVR
  return v;
}

ISR(TIMER0_COMPA_vect) { _ticks_ms++; }
