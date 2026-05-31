#pragma once

#include <avr/io.h>
#include <stdint.h>

// ── USART → HC-06 Bluetooth ─────────────────────────────────────────
// 9600 baud, 8N1, double-speed (U2X0). UBRR is derived from F_CPU, so this
// works unchanged at 1/8/16 MHz (UBRR = 12 / 103 / 207 respectively).
// Pins are fixed by hardware: PD1 = TXD (-> HC-06 RXD via divider),
//                             PD0 = RXD (<- HC-06 TXD direct).

#define UART_BAUD 9600UL
#define UART_UBRR ((F_CPU / (8UL * UART_BAUD)) - 1)   // double-speed formula

void uart_init(void) {
    UBRR0H = (uint8_t)(UART_UBRR >> 8);
    UBRR0L = (uint8_t)(UART_UBRR & 0xFF);
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

// send an unsigned value as plain decimal (no leading zeros)
void uart_put_u32(uint32_t v) {
    char buf[10];
    uint8_t i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i > 0) { uart_putc(buf[--i]); }   // digits came out reversed
}

// non-blocking receive: returns the byte (0-255) if one arrived, else -1
int16_t uart_try_getc(void) {
    if (UCSR0A & (1 << RXC0)) { return UDR0; }   // RXC0 = byte waiting
    return -1;
}
