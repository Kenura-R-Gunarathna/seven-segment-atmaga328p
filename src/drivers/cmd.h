/*
 * UART command parser.
 *
 * Commands (terminated by CR or LF):
 *   f<1-20000>  set output frequency in Hz         "f1000"
 *   w<0-3>      set waveform: 0=sin 1=sqr 2=tri 3=saw
 *   s           print status
 *   m<0-2>      set mode: 0=FG only  1=OSC only  2=BOTH
 *   o           single oscilloscope measurement     → "V: +1.234V"
 *   os<1-1000>  stream n oscilloscope samples       → millivolts per line
 *
 * Responses: "OK ...\r\n" on success, "ERR ...\r\n" on bad argument.
 */

#pragma once

#include <stdint.h>
#include "uart.h"
#include "dds.h"
#include "led_status.h"
#include "mode.h"
#include "sar_osc.h"
#include "adc_int.h"

#define _CMD_BUF 16

static char     _cbuf[_CMD_BUF];
static uint8_t  _clen      = 0;
static uint32_t _cmd_freq  = 1000;
static uint8_t  _cmd_wave  = 0;

static const char * const _wname[4] = {"sin", "sqr", "tri", "saw"};
static const led_state_t  _wled[4]  = {
    LS_SINE, LS_SQUARE, LS_TRIANGLE, LS_SAWTOOTH
};

static void _cmd_exec(void)
{
    if (_clen == 0) return;

    char op = _cbuf[0];

    /* ── f<hz> ── */
    if (op == 'f' || op == 'F') {
        uint32_t hz = 0;
        for (uint8_t i = 1; i < _clen; i++) {
            if (_cbuf[i] >= '0' && _cbuf[i] <= '9')
                hz = hz * 10u + (uint32_t)(_cbuf[i] - '0');
        }
        uint32_t max_hz = (mode_get() == MODE_BOTH) ? 10000u : 20000u;
        if (hz < 1 || hz > max_hz) {
            uart_puts("ERR f 1-"); uart_put_u32(max_hz); uart_puts("\r\n");
        } else {
            _cmd_freq = hz;
            if (mode_get() == MODE_BOTH)
                dds_set_freq_raw(hz, DDS_RATE_BOTH);
            else
                dds_set_freq(hz);
            led_set_state(_wled[_cmd_wave], hz);
            uart_puts("OK f"); uart_put_u32(hz); uart_puts("\r\n");
        }

    /* ── w<0-3> ── */
    } else if (op == 'w' || op == 'W') {
        if (_clen < 2 || _cbuf[1] < '0' || _cbuf[1] > '3') {
            uart_puts("ERR w 0-3\r\n");
        } else {
            _cmd_wave = (uint8_t)(_cbuf[1] - '0');
            dds_set_wave(_cmd_wave);
            led_set_state(_wled[_cmd_wave], _cmd_freq);
            uart_puts("OK w"); uart_putc(_cbuf[1]); uart_puts("\r\n");
        }

    /* ── m<0-2> ── */
    } else if (op == 'm' || op == 'M') {
        if (_clen < 2 || _cbuf[1] < '0' || _cbuf[1] > '2') {
            uart_puts("ERR m 0-2\r\n");
        } else {
            op_mode_t m = (op_mode_t)(_cbuf[1] - '0');
            mode_set(m);
            const char * const _mname[3] = {"fg", "osc", "both"};
            uart_puts("OK m"); uart_puts(_mname[m]); uart_puts("\r\n");
        }

    /* ── o / os<n> / oi / oi<n> ── */
    } else if (op == 'o' || op == 'O') {
        if (_clen >= 2 && (_cbuf[1] == 'i' || _cbuf[1] == 'I')) {
            if (_clen == 2) {
                /* oi: single fast ADC reading */
                adc_int_single();
            } else {
                /* oi<n>: stream n fast ADC samples */
                uint16_t n = 0;
                for (uint8_t i = 2; i < _clen; i++) {
                    if (_cbuf[i] >= '0' && _cbuf[i] <= '9')
                        n = (uint16_t)(n * 10u + (_cbuf[i] - '0'));
                }
                if (n < 1 || n > 1000) {
                    uart_puts("ERR oi 1-1000\r\n");
                } else {
                    adc_int_stream_n(n);
                }
            }
        } else if (_clen >= 2 && (_cbuf[1] == 's' || _cbuf[1] == 'S')) {
            /* os<n>: stream n SAR (16-bit) samples */
            uint16_t n = 0;
            for (uint8_t i = 2; i < _clen; i++) {
                if (_cbuf[i] >= '0' && _cbuf[i] <= '9')
                    n = (uint16_t)(n * 10u + (_cbuf[i] - '0'));
            }
            if (n < 1 || n > 1000) {
                uart_puts("ERR os 1-1000\r\n");
            } else {
                sar_stream_n(n);
            }
        } else {
            /* o: single SAR precision measurement */
            sar_single();
        }

    /* ── s ── */
    } else if (op == 's' || op == 'S') {
        const char * const _mname[3] = {"fg", "osc", "both"};
        uart_puts("f"); uart_put_u32(_cmd_freq);
        uart_puts(" w"); uart_putc('0' + _cmd_wave);
        uart_puts(" "); uart_puts(_wname[_cmd_wave]);
        uart_puts(" m"); uart_puts(_mname[mode_get()]);
        uart_puts("\r\n");

    /* ── d<hex4> ── DAC test: write raw 16-bit hex value to SAR DAC
     *   d0000 → 0V   d8000 → 2.5V   dffff → 5V  */
    } else if (op == 'd' || op == 'D') {
        uint16_t val = 0;
        for (uint8_t i = 1; i < _clen; i++) {
            uint8_t nib;
            char c = _cbuf[i];
            if      (c >= '0' && c <= '9') nib = (uint8_t)(c - '0');
            else if (c >= 'a' && c <= 'f') nib = (uint8_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') nib = (uint8_t)(c - 'A' + 10);
            else break;
            val = (uint16_t)((val << 4) | nib);
        }
        dac16_write(val);
        uart_puts("DAC 0x");
        /* print 4-digit hex */
        for (int8_t sh = 12; sh >= 0; sh -= 4) {
            uint8_t nib = (uint8_t)((val >> sh) & 0xF);
            uart_putc(nib < 10 ? '0' + nib : 'A' + nib - 10);
        }
        uart_puts("\r\n");

    } else {
        uart_puts("ERR: f<hz> w<0-3> m<0-2> o os<n> oi oi<n> d<hex4> s\r\n");
    }

    _clen = 0;
}

void cmd_poll(void)
{
    int16_t ch = uart_try_getc();
    if (ch < 0) return;

    if (ch == '\r' || ch == '\n') {
        _cmd_exec();
    } else if (_clen < _CMD_BUF - 1) {
        _cbuf[_clen++] = (char)ch;
        _cbuf[_clen]   = 0;
    }
}
