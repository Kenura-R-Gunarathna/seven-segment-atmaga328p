#pragma once

#include <avr/io.h>
#include <stdint.h>
#include "uart.h"

/*
 * ATmega32A internal 10-bit ADC driver.
 *
 * Connects to same biased/clamped input node as NE5532-B pin 5.
 * PA0 is the default channel (ADC0).
 *
 * 2.5V bias decoding (same scheme as SAR):
 *   raw = 0   → probe = −V_FS (0V at comparator)
 *   raw = 511 → probe = 0V
 *   raw = 1023→ probe = +V_FS (5V at comparator)
 *
 * ADC clock prescaler options (at 16MHz F_CPU):
 *   ADC_PS_128  → 125kHz ADC clock → ~9.6kSPS  (full 10-bit accuracy)
 *   ADC_PS_64   → 250kHz           → ~19kSPS   (full 10-bit accuracy)
 *   ADC_PS_32   → 500kHz           → ~38kSPS   (~9.5 effective bits)
 *   ADC_PS_16   → 1MHz             → ~77kSPS   (~8 effective bits)
 *
 * Default: ADC_PS_32 → good tradeoff: 38kSPS, still useful for campus scope.
 */

#define ADC_INT_CHANNEL      0       /* PA0 = ADC0 */
#define ADC_INT_V_FS_MV   2500       /* ±2500mV full scale (same as SAR) */

#define ADC_PS_16    ((1<<ADPS2))
#define ADC_PS_32    ((1<<ADPS2)|(1<<ADPS0))
#define ADC_PS_64    ((1<<ADPS2)|(1<<ADPS1))
#define ADC_PS_128   ((1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0))

void adc_int_init(void) {
    DDRA  &= ~(1 << ADC_INT_CHANNEL);  /* PA0 as input */
    PORTA &= ~(1 << ADC_INT_CHANNEL);  /* no pull-up */
    /* AVCC reference, right-adjust result, channel 0 */
    ADMUX = (1 << REFS0) | ADC_INT_CHANNEL;
    /* Enable ADC, prescaler /32 → ~38kSPS */
    ADCSRA = (1 << ADEN) | ADC_PS_32;
    /* Discard first conversion after enable */
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
}

static inline uint16_t adc_int_read(void) {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;   /* 0–1023 */
}

/* Decode raw ADC value to millivolts (centred at 511 = 0V probe). */
static inline int32_t adc_int_to_mv(uint16_t raw) {
    return (int32_t)((int16_t)raw - 511) * ADC_INT_V_FS_MV / 511L;
}

/* Stream n fast samples as millivolts: "+1234\r\n" per line.
 * At 38kSPS the ADC is ready long before UART can keep up at 9600 baud.
 * Recommend increasing baud to 115200 in uart.h for large captures. */
void adc_int_stream_n(uint16_t n) {
    for (uint16_t i = 0; i < n; i++) {
        int32_t mv = adc_int_to_mv(adc_int_read());
        if (mv < 0) { uart_putc('-'); mv = -mv; } else { uart_putc('+'); }
        uart_put_u32((uint32_t)mv);
        uart_puts("\r\n");
    }
}

/* Single reading with label: "ADC: +1.234V\r\n" */
void adc_int_single(void) {
    int32_t mv = adc_int_to_mv(adc_int_read());
    uart_puts("ADC: ");
    if (mv < 0) { uart_putc('-'); mv = -mv; } else { uart_putc('+'); }
    uart_put_u32((uint32_t)(mv / 1000));
    uart_putc('.');
    uint16_t frac = (uint16_t)(mv % 1000);
    if (frac < 100) uart_putc('0');
    if (frac < 10)  uart_putc('0');
    uart_put_u32((uint32_t)frac);
    uart_puts("V\r\n");
}
