#pragma once

#include <stdint.h>
#include "uart.h"

// ── HC-06 Bluetooth serial driver ───────────────────────────────────
// HC-06 is a transparent UART<->Bluetooth SPP bridge running at 9600 baud.
// This driver builds the cap-meter protocol on top of the raw UART driver.
// See uart.h for pin wiring (PD0 RX, PD1 TX via 1k/2k divider).

// Send the device-ready greeting.
void hc06_ready(void) {
    uart_puts("READY\n");
}

// Stream one measurement result line:
//   "Rch (R ohm):\n ... raw ticks ...\navg dt=X min=Y max=Z med=W  C=V pF\n"
// (Callers send the raw ticks and the summary line via uart_put_u32/uart_puts
//  directly — this file provides the helper wrappers.)

// Non-blocking poll for a received byte. Returns the byte or -1.
static inline int16_t hc06_getc(void) {
    return uart_try_getc();
}

// Check whether a received byte is a "trigger measurement" command.
// Protocol: 'm', 'M', CR, or LF trigger a measurement.
static inline uint8_t hc06_is_trigger(int16_t c) {
    return (c == 'm' || c == 'M' || c == '\r' || c == '\n');
}

// Re-export the low-level send helpers under the hc06_ namespace so
// main.c only needs to #include this one header.
#define hc06_putc       uart_putc
#define hc06_puts       uart_puts
#define hc06_put_u32    uart_put_u32
