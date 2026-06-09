/**
 * @file    uart.h
 * @brief   USART driver for HC-06 Bluetooth on ATmega32A.
 *
 * Configuration
 * -------------
 *   Baud rate : 9600  (double-speed mode, U2X=1)
 *   Format    : 8N1
 *   Pins      : PD0 = RXD  (HC-06 TXD)
 *               PD1 = TXD  (HC-06 RXD, via voltage divider on PCB)
 *
 * ATmega32A note
 * --------------
 * UCSRC and UBRRH share the same I/O address (0x40).
 *   Writing with bit 7 = 0  -> selects UBRRH
 *   Writing with bit 7 = 1  -> selects UCSRC  (URSEL flag)
 * Always set URSEL when writing UCSRC to avoid corrupting UBRRH.
 */

#pragma once

#include <avr/io.h>
#include <stdint.h>

/** UART baud rate. Change here to reconfigure; UBRR is auto-derived. */
#define UART_BAUD 9600UL

/** Baud-rate register value for double-speed mode (U2X=1). */
#define UART_UBRR ((F_CPU / (8UL * UART_BAUD)) - 1)

/**
 * @brief Initialise the USART: baud rate, double-speed, 8N1, TX+RX enabled.
 *        Must be called before any uart_putc / uart_try_getc calls.
 */
void uart_init(void) {
    UBRRH = (uint8_t)(UART_UBRR >> 8);                   /* URSEL=0 -> UBRRH      */
    UBRRL = (uint8_t)(UART_UBRR & 0xFF);
    UCSRA = (1 << U2X);                                   /* double-speed mode     */
    UCSRB = (1 << TXEN) | (1 << RXEN);                   /* enable TX and RX      */
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0); /* URSEL=1 -> 8 data bits */
}

/**
 * @brief Transmit one byte. Blocks until the transmit buffer is empty.
 * @param c  Byte to send.
 */
void uart_putc(char c) {
    while (!(UCSRA & (1 << UDRE))) { }
    UDR = c;
}

/**
 * @brief Transmit a null-terminated string.
 * @param s  Pointer to string in SRAM.
 */
void uart_puts(const char* s) {
    while (*s) { uart_putc(*s++); }
}

/**
 * @brief Transmit a value as a fixed 2-digit decimal with leading zero.
 *        e.g. 7 -> "07",  42 -> "42"
 * @param v  Value to send (0-99).
 */
void uart_put_2d(uint8_t v) {
    uart_putc('0' + (v / 10) % 10);
    uart_putc('0' + v % 10);
}

/**
 * @brief Transmit an unsigned 32-bit value as plain decimal, no leading zeros.
 *        e.g. 1000 -> "1000"
 * @param v  Value to send.
 */
void uart_put_u32(uint32_t v) {
    char buf[10];
    uint8_t i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i > 0) { uart_putc(buf[--i]); } /* digits stored in reverse order */
}

/**
 * @brief Non-blocking receive. Returns the waiting byte, or -1 if none.
 * @return  Received byte (0-255) cast to int16_t, or -1 if RX buffer empty.
 */
int16_t uart_try_getc(void) {
    if (UCSRA & (1 << RXC)) { return UDR; }
    return -1;
}
