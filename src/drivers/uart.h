#pragma once

#include <avr/io.h>
#include <stdint.h>

// ── USART → HC-06 Bluetooth ─────────────────────────────────────────
// 9600 baud, 8N1. At F_CPU=1MHz plain 9600 has ~7% error, so we use
// double-speed (U2X0) with UBRR=12 -> 9615 baud, 0.16% error (reliable).
// Pins are fixed by hardware: PD1 = TXD (-> HC-06 RXD via divider),
//                             PD0 = RXD (<- HC-06 TXD direct).

void uart_init(void) {
    UBRR0H = 0;
    UBRR0L = 12;                          // 9600 @ 1MHz with U2X0
    UCSR0A = (1 << U2X0);                 // double speed
    UCSR0B = (1 << TXEN0) | (1 << RXEN0); // enable transmit + receive
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8 data bits, no parity, 1 stop
}

// send one byte (blocks until the transmit buffer is free)
void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0))) { }   // wait: data register empty?
    UDR0 = c;                              // hand the byte to the hardware
}

// send a null-terminated string
void uart_puts(const char* s) {
    while (*s) { uart_putc(*s++); }
}

// send a value as a fixed 2-digit decimal (e.g. 7 -> "07")
void uart_put_2d(uint8_t v) {
    uart_putc('0' + (v / 10) % 10);
    uart_putc('0' + v % 10);
}

// non-blocking receive: returns the byte (0-255) if one arrived, else -1
int16_t uart_try_getc(void) {
    if (UCSR0A & (1 << RXC0)) { return UDR0; }   // RXC0 = byte waiting
    return -1;
}
