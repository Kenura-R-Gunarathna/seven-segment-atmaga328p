#pragma once

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "dac16.h"
#include "uart.h"

/*
 * 16-bit SAR ADC oscilloscope using NE5532-B as open-loop comparator.
 *
 * Comparator → 6N137 → ATmega PD2:
 *   Signal > DAC: NE5532-B output HIGH (+13V) → 6N137 LED ON → PD2 LOW
 *   Signal < DAC: NE5532-B output LOW (−13V)  → 6N137 LED OFF → PD2 HIGH
 *   (6N137 output is active-low: LED on → collector pulls down → PD2 = LOW)
 *
 * Encoding: 0x8000 = 0V (2.5V at comparator input), 0x0000 = −V_FS, 0xFFFF = +V_FS
 *
 * SAR_V_FULLSCALE_MV: full-scale range in millivolts (default ±2500 mV = ±2.5V).
 * For ±10V range: change to 10000 AND add ÷4 resistor divider on probe input.
 */

#define SAR_CMP_BIT         PD2
#define SAR_V_FULLSCALE_MV  2500

void sar_init(void) {
    DDRD  &= ~(1 << SAR_CMP_BIT);
    PORTD &= ~(1 << SAR_CMP_BIT);
    dac16_write(0x8000);   /* pre-charge DAC to mid-rail */
}

uint16_t sar_convert(void) {
    uint16_t result = 0;
    for (int8_t bit = 15; bit >= 0; bit--) {
        result |= (uint16_t)(1u << bit);
        dac16_write(result);
        _delay_us(10);
        /* PD2 HIGH = 6N137 off = DAC > signal → clear this bit */
        if (PIND & (1 << SAR_CMP_BIT))
            result &= (uint16_t)~(1u << bit);
    }
    return result;
}

static void _sar_print_mv(int32_t mv) {
    if (mv < 0) { uart_putc('-'); mv = -mv; } else { uart_putc('+'); }
    uart_put_u32((uint32_t)(mv / 1000));
    uart_putc('.');
    uint16_t frac = (uint16_t)(mv % 1000);
    if (frac < 100) uart_putc('0');
    if (frac < 10)  uart_putc('0');
    uart_put_u32((uint32_t)frac);
    uart_putc('V');
}

/* Single measurement with human-readable output: "V: +1.234V\r\n" */
void sar_single(void) {
    uint16_t raw = sar_convert();
    int32_t mv = (int32_t)(int16_t)(raw - 0x8000u)
                 * (int32_t)SAR_V_FULLSCALE_MV / 32768L;
    uart_puts("V: ");
    _sar_print_mv(mv);
    uart_puts("\r\n");
}

/* Stream n samples as millivolts integers: "+1234\r\n" per line.
 * At 9600 baud each line takes ~5ms — keep n ≤ 200 for responsive output.
 * For large captures increase UART baud in uart.h first. */
void sar_stream_n(uint16_t n) {
    for (uint16_t i = 0; i < n; i++) {
        uint16_t raw = sar_convert();
        int32_t mv = (int32_t)(int16_t)(raw - 0x8000u)
                     * (int32_t)SAR_V_FULLSCALE_MV / 32768L;
        if (mv < 0) { uart_putc('-'); mv = -mv; } else { uart_putc('+'); }
        uart_put_u32((uint32_t)mv);
        uart_puts("\r\n");
    }
}
