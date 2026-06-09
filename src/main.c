/*
 * Phase 2.0 -- DDS function generator + 16-bit SAR oscilloscope, 3 modes.
 *
 * UART commands:
 *   f<hz>      set frequency 1-20000 Hz (1-10000 in BOTH mode)
 *   w<0-3>     set waveform: 0=sin 1=sqr 2=tri 3=saw
 *   m<0-2>     mode: 0=FG only  1=OSC only  2=BOTH
 *   o          single SAR voltage measurement
 *   os<n>      stream n SAR samples (millivolts per line, n 1-1000)
 *   oi         single internal ADC reading
 *   oi<n>      stream n internal ADC samples
 *   d<hex4>    write raw 16-bit hex to SAR DAC (test)
 *   s          status report
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "drivers/led_status.h"
#include "drivers/dds.h"
#include "drivers/uart.h"
#include "drivers/dac16.h"
#include "drivers/sar_osc.h"
#include "drivers/adc_int.h"
#include "drivers/cmd.h"

extern void millis_init(void);

int main(void) {
    led_boot_signature();
    millis_init();
    led_init();
    dds_init();
    dac16_init();
    sar_init();
    adc_int_init();
    uart_init();
    sei();

    dds_set_wave(0);
    dds_set_freq(1000);
    led_set_state(LS_SINE, 1000);
    uart_puts("FG f1000 w0 m0\r\n");

    for (;;) { led_update(); cmd_poll(); }
}
