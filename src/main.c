// main.c — Capacitance meter, Phase 2: CD4067 auto-ranging.
// Picks the range resistor automatically so the charge time lands in Timer1's
// good window, averages N trimmed samples, computes C = dt*K/R + b, shows it
// (with unit) on the display and streams it over Bluetooth.
//
// Wiring (see CAP_METER.md):
//   PB0 -> charge  PD6 -> AIN0 (node, comp +)  PC4/PC3 -> V_low/V_high (comp - via mux)
//   PD2 -> button  PD0/PD1 -> HC-06
//   CD4067: COM <- PB0, channels I0..I10 -> measured resistors -> node,
//           S0..S3 <- PC0,PC1,PC2,PD7,  INH -> GND
// Clock: 8 MHz internal. Timer1 ps=8 -> 1 us/tick.

#include <avr/interrupt.h>
#include <avr/io.h>
#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/scroll.h"
#include "drivers/comparator.h"
#include "drivers/uart.h"

#define REF_LOW_CH   4
#define REF_HIGH_CH  3
#define T1_PS_BITS   (1 << CS11)   // /8 -> 1 us/tick @ 8 MHz

#define N_SAMPLES    30
#define N_TRIM       3
#define DT_MIN       200U          // below -> bigger R (too fast / quantized)
#define DT_MAX       58000U        // above -> smaller R (slow; <65535 wrap)
#define MEAS_TIMEOUT 200UL         // ms backstop for a single reading
#define DISCHARGE_MAX 500UL        // ms cap on adaptive discharge

// Quick test: lock to one channel (set to -1 to re-enable auto-ranging).
#define FORCE_RANGE  -1

// Calibration: C[pF] = dt * CAL_K / R + CAL_B.
// From C7 2-point fit (10nF dt=733, 100nF dt=7788): a=12.76 pF/tick, b=650 pF.
// CAL_K = a*R = 12.76 * 99300.  Refine with the Phase-3 sim across ranges.
#define CAL_K        1266700UL
#define CAL_B        650L

// Measured channel resistors (ohms), C0..C10. C11..C15 unpopulated.
static const uint32_t MUX_R[] = {
    10UL, 56UL, 554UL, 980UL, 5519UL, 9860UL,
    56370UL, 99300UL, 560000UL, 1050000UL, 5600000UL
};
#define NUM_RANGES (sizeof(MUX_R) / sizeof(MUX_R[0]))

static const GPIO CHARGE = { &DDRB, &PORTB, &PINB, PB0 };
static const GPIO BUTTON = { &DDRD, &PORTD, &PIND, PD2 };

static uint8_t g_range = 7;        // remembered range (start mid)

volatile uint16_t t_low      = 0;
volatile uint16_t t_high     = 0;
volatile uint8_t  meas_state = 0;

ISR(TIMER1_CAPT_vect) {
    if (meas_state == 0) {
        t_low = ICR1;
        comp_use_adc_pin(REF_HIGH_CH);
        TIFR1 |= (1 << ICF1);
        meas_state = 1;
    } else if (meas_state == 1) {
        t_high = ICR1;
        TIMSK1 &= ~(1 << ICIE1);
        meas_state = 2;
    }
}

static void timer1_init(void) {
    TCCR1A = 0;
    TCCR1B = (1 << ICES1) | T1_PS_BITS;
    TIMSK1 = 0;
}

// CD4067 channel select: S0..S2 = PC0..PC2, S3 = PD7
static void mux_select(uint8_t ch) {
    PORTC = (PORTC & ~0x07) | (ch & 0x07);
    if (ch & 0x08) { PORTD |=  (1 << PD7); }
    else           { PORTD &= ~(1 << PD7); }
}

// One charge-time measurement on the current range. Returns dt ticks, or 0 on
// timeout/overflow (cap too big for this R). Discharge is adaptive: it waits
// until the node has fallen below V_low, so it works for any R*C.
static uint16_t measure_once(void) {
    comp_use_bandgap(0);                         // + = AIN0 (node)
    comp_use_adc_pin(REF_LOW_CH);                // - = V_low
    gpio_low(CHARGE);                            // discharge
    uint32_t t0 = millis();
    while (comp_read() && millis() - t0 < DISCHARGE_MAX) { display_refresh(); }
    t0 = millis();
    while (millis() - t0 < 2) { display_refresh(); }   // small settle

    meas_state = 0;
    comp_use_adc_pin(REF_LOW_CH);
    TCNT1 = 0;
    TIFR1 |= (1 << ICF1) | (1 << TOV1);          // clear capture + overflow
    TIMSK1 |= (1 << ICIE1);
    gpio_high(CHARGE);

    uint32_t ts = millis();
    while (meas_state != 2) {
        display_refresh();
        if (TIFR1 & (1 << TOV1)) {               // timer wrapped -> too slow
            TIMSK1 &= ~(1 << ICIE1); gpio_low(CHARGE); return 0;
        }
        if (millis() - ts > MEAS_TIMEOUT) {      // open / no cap
            TIMSK1 &= ~(1 << ICIE1); gpio_low(CHARGE); return 0;
        }
    }
    gpio_low(CHARGE);
    return t_high - t_low;
}

// Step the range until a single reading lands in [DT_MIN, DT_MAX].
// Larger R (higher ch) -> longer dt. Returns chosen channel.
static uint8_t auto_range(void) {
    uint8_t ch = g_range;
    for (uint8_t tries = 0; tries < NUM_RANGES + 2; tries++) {
        mux_select(ch);
        uint16_t d = measure_once();
        if (d == 0 || d > DT_MAX) {              // too slow / overflow -> smaller R
            if (ch > 0) { ch--; continue; } else break;
        }
        if (d < DT_MIN) {                        // too fast -> bigger R
            if (ch < NUM_RANGES - 1) { ch++; continue; } else break;
        }
        break;                                   // in window
    }
    g_range = ch;
    return ch;
}

// display formatting (defined after main)
static char* u32_to_str(char* p, uint32_t v);
static char  capstr[20];
static void  scroll_cap(uint32_t pf);

static void run_measurement(void) {
    scroll_stop();

#if FORCE_RANGE >= 0
    uint8_t ch = FORCE_RANGE;            // locked range (quick test)
    g_range = ch;
#else
    uint8_t ch = auto_range();           // normal auto-ranging
#endif
    mux_select(ch);

    uint16_t s[N_SAMPLES];
    uint16_t cnt = 0, mn = 0xFFFF, mx = 0;
    uart_puts("R"); uart_put_u32(ch);
    uart_puts(" ("); uart_put_u32(MUX_R[ch]); uart_puts(" ohm):\n");

    for (uint16_t i = 0; i < N_SAMPLES; i++) {
        display_write((uint16_t)((N_SAMPLES - i) * 100UL / N_SAMPLES)); // % left
        uint16_t d = measure_once();
        if (d) {
            s[cnt++] = d;
            if (d < mn) mn = d;
            if (d > mx) mx = d;
            uart_put_u32(d); uart_putc(' ');
        }
    }
    uart_putc('\n');

    if (cnt) {
        for (uint8_t a = 1; a < cnt; a++) {      // insertion sort
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

        uart_puts("avg dt="); uart_put_u32(avg);
        uart_puts(" min=");   uart_put_u32(mn);
        uart_puts(" max=");   uart_put_u32(mx);
        uart_puts(" med=");   uart_put_u32(s[cnt / 2]);
        uart_puts("  C=");    uart_put_u32(c_pf);
        uart_puts(" pF\n");
        scroll_cap(c_pf);
    } else {
        uart_puts("no cap / out of range\n");
        scroll_start("    no CAP    ", 300);
    }
}

int main(void) {
    gpio_set_all_output();
    millis_init();
    uart_init();
    sei();

    gpio_init(CHARGE, OUTPUT, LOW);
    gpio_init(BUTTON, INPUT, HIGH);
    DDRC |= 0x07;                                // S0..S2 outputs (PC0..PC2)
    DDRD |= (1 << PD7);                          // S3 output
    mux_select(g_range);

    comp_enable();
    ACSR |= (1 << ACIC);                         // comparator -> Timer1 capture
    timer1_init();

    uart_puts("READY\n");
    scroll_start("    MEASURE    ", 300);
    uint8_t last_sw = HIGH;

    for (;;) {
        display_refresh();
        scroll_tick();

        uint8_t sw = gpio_read(BUTTON);
        int16_t rx = uart_try_getc();
        uint8_t go = (sw == LOW && last_sw == HIGH)
                  || (rx == 'm' || rx == 'M' || rx == '\r' || rx == '\n');
        last_sw = sw;

        if (go) {
            run_measurement();
            while (gpio_read(BUTTON) == LOW) { display_refresh(); }
        }
    }
    return 0;
}

// write a uint32 as decimal into p, return new end pointer
static char* u32_to_str(char* p, uint32_t v) {
    char tmp[10]; uint8_t n = 0;
    if (v == 0) { *p++ = '0'; return p; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    while (n) { *p++ = tmp[--n]; }
    return p;
}

// scroll the capacitance with an auto unit: pF / nF (x.x) / uF (x.xx)
static void scroll_cap(uint32_t pf) {
    char* p = capstr;
    *p++ = ' '; *p++ = ' ';
    if (pf < 1000UL) {                           // pF
        p = u32_to_str(p, pf);
        *p++ = ' '; *p++ = 'P'; *p++ = 'F';
    } else if (pf < 1000000UL) {                 // nF, one decimal
        uint32_t nf10 = (pf + 50UL) / 100UL;
        p = u32_to_str(p, nf10 / 10);
        *p++ = '.'; p = u32_to_str(p, nf10 % 10);
        *p++ = ' '; *p++ = 'N'; *p++ = 'F';
    } else {                                     // uF, two decimals
        uint32_t uf100 = (pf + 5000UL) / 10000UL;
        p = u32_to_str(p, uf100 / 100);
        *p++ = '.';
        *p++ = '0' + (uf100 % 100) / 10;
        *p++ = '0' + (uf100 % 100) % 10;
        *p++ = ' '; *p++ = 'U'; *p++ = 'F';
    }
    *p++ = ' '; *p++ = ' '; *p = '\0';
    scroll_start(capstr, 300);
}
