#pragma once

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include "gpio.h"
#include "comparator.h"
#include "millis.h"
#include "display.h"
#include "scroll.h"
#include "mux4067.h"    // MUX_R[], CAL_K, CAL_B, auto_range()
#include "hc06.h"

// ── Capacitance measurement driver ──────────────────────────────────
// Handles: Timer1 input-capture, discharge/charge cycle, trimmed-mean
// averaging, C=dt*K/R+b computation, display + Bluetooth output.

#define REF_LOW_CH    4            // PC4 = 1/3 Vcc (comparator -)
#define REF_HIGH_CH   3            // PC3 = 2/3 Vcc
#define T1_PS_BITS    (1 << CS11) // /8 -> 0.5 us/tick @ 16 MHz crystal

#define N_SAMPLES     30
#define N_TRIM        3
#define MEAS_TIMEOUT  200UL        // ms backstop per single reading
#define DISCHARGE_MAX 500UL        // ms cap on adaptive discharge

static const GPIO CHARGE = { &DDRB, &PORTB, &PINB, PB0 };

// ── Timer1 input-capture ISR ─────────────────────────────────────────
volatile uint16_t t_low      = 0;
volatile uint16_t t_high     = 0;
volatile uint8_t  meas_state = 0;   // 0=wait Vlo, 1=wait Vhi, 2=done

ISR(TIMER1_CAPT_vect) {
    if (meas_state == 0) {
        t_low = ICR1;
        comp_use_adc_pin(REF_HIGH_CH);   // switch ref to Vhi
        TIFR1 |= (1 << ICF1);
        meas_state = 1;
    } else if (meas_state == 1) {
        t_high = ICR1;
        TIMSK1 &= ~(1 << ICIE1);
        meas_state = 2;
    }
}

void capmeas_init(void) {
    gpio_init(CHARGE, OUTPUT, LOW);
    TCCR1A = 0;
    TCCR1B = (1 << ICES1) | T1_PS_BITS;   // rising-edge capture
    TIMSK1 = 0;                            // armed per-measurement
    comp_enable();
    ACSR |= (1 << ACIC);                   // comparator -> Timer1 capture
}

// One discharge-then-charge cycle. Returns dt ticks, or 0 on timeout/overflow.
static uint16_t measure_once(void) {
    comp_use_bandgap(0);                   // + = AIN0 (node)
    comp_use_adc_pin(REF_LOW_CH);          // - = Vlo
    gpio_low(CHARGE);                      // discharge
    uint32_t t0 = millis();
    while (comp_read() && millis() - t0 < DISCHARGE_MAX) { display_refresh(); }
    t0 = millis();
    while (millis() - t0 < 2) { display_refresh(); }   // settle

    meas_state = 0;
    comp_use_adc_pin(REF_LOW_CH);
    TCNT1 = 0;
    TIFR1 |= (1 << ICF1) | (1 << TOV1);
    TIMSK1 |= (1 << ICIE1);
    gpio_high(CHARGE);

    uint32_t ts = millis();
    while (meas_state != 2) {
        display_refresh();
        if (TIFR1 & (1 << TOV1)) {         // overflow -> too slow for this R
            TIMSK1 &= ~(1 << ICIE1); gpio_low(CHARGE); return 0;
        }
        if (millis() - ts > MEAS_TIMEOUT) {
            TIMSK1 &= ~(1 << ICIE1); gpio_low(CHARGE); return 0;
        }
    }
    gpio_low(CHARGE);
    return t_high - t_low;
}

// ── Display formatting ───────────────────────────────────────────────
static char* _u32_to_str(char* p, uint32_t v) {
    char tmp[10]; uint8_t n = 0;
    if (v == 0) { *p++ = '0'; return p; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    while (n) { *p++ = tmp[--n]; }
    return p;
}

static char _capstr[20];

// Scroll the result on the 7-seg with auto unit: pF / x.x nF / x.xx uF.
static void scroll_cap(uint32_t pf) {
    char* p = _capstr;
    *p++ = ' '; *p++ = ' ';
    if (pf < 1000UL) {
        p = _u32_to_str(p, pf);
        *p++ = ' '; *p++ = 'P'; *p++ = 'F';
    } else if (pf < 1000000UL) {
        uint32_t nf10 = (pf + 50UL) / 100UL;
        p = _u32_to_str(p, nf10 / 10);
        *p++ = '.'; p = _u32_to_str(p, nf10 % 10);
        *p++ = ' '; *p++ = 'N'; *p++ = 'F';
    } else {
        uint32_t uf100 = (pf + 5000UL) / 10000UL;
        p = _u32_to_str(p, uf100 / 100);
        *p++ = '.';
        *p++ = '0' + (uf100 % 100) / 10;
        *p++ = '0' + (uf100 % 100) % 10;
        *p++ = ' '; *p++ = 'U'; *p++ = 'F';
    }
    *p++ = ' '; *p++ = ' '; *p = '\0';
    scroll_start(_capstr, 300);
}

// ── Full averaged measurement + report ──────────────────────────────
void capmeas_run(void) {
    scroll_stop();

#if FORCE_RANGE >= 0
    uint8_t ch = FORCE_RANGE;
    g_range = ch;
#else
    uint8_t ch = auto_range();
#endif
    mux_select(ch);

    uint16_t s[N_SAMPLES];
    uint16_t cnt = 0, mn = 0xFFFF, mx = 0;
    hc06_puts("R"); hc06_put_u32(ch);
    hc06_puts(" ("); hc06_put_u32(MUX_R[ch]); hc06_puts(" ohm):\n");

    for (uint16_t i = 0; i < N_SAMPLES; i++) {
        display_write((uint16_t)((N_SAMPLES - i) * 100UL / N_SAMPLES));
        uint16_t d = measure_once();
        if (d) {
            s[cnt++] = d;
            if (d < mn) mn = d;
            if (d > mx) mx = d;
            hc06_put_u32(d); hc06_putc(' ');
        }
    }
    hc06_putc('\n');

    if (cnt) {
        for (uint8_t a = 1; a < cnt; a++) {   // insertion sort for trimming
            uint16_t key = s[a]; int8_t b = a - 1;
            while (b >= 0 && s[b] > key) { s[b + 1] = s[b]; b--; }
            s[b + 1] = key;
        }
        uint8_t lo = 0, hi = cnt;
        if (cnt > 2 * N_TRIM) { lo = N_TRIM; hi = cnt - N_TRIM; }
        uint32_t sum = 0;
        for (uint8_t i = lo; i < hi; i++) { sum += s[i]; }
        uint16_t avg = (uint16_t)(sum / (hi - lo));

        int32_t c = (int32_t)((uint64_t)avg * CAL_K / MUX_R[ch]) + CAL_B;
        if (c < 0) { c = 0; }
        uint32_t c_pf = (uint32_t)c;

        hc06_puts("avg dt="); hc06_put_u32(avg);
        hc06_puts(" min=");   hc06_put_u32(mn);
        hc06_puts(" max=");   hc06_put_u32(mx);
        hc06_puts(" med=");   hc06_put_u32(s[cnt / 2]);
        hc06_puts("  C=");    hc06_put_u32(c_pf);
        hc06_puts(" pF\n");
        scroll_cap(c_pf);
    } else {
        hc06_puts("no cap / out of range\n");
        scroll_start("    no CAP    ", 300);
    }
}
