/**
 * @file    led_status.h
 * @brief   Non-blocking 3-LED status indicator for ATmega32A.
 *
 * LED roles
 * ---------
 *   PB5  System heartbeat  — always blinking; rate changes with activity
 *   PD6  Wave type         — pulse count encodes waveform shape
 *   PD7  Frequency / Comms — blink speed encodes freq range or comms type
 *
 * Pattern encoding (PD6 pulse count tells the wave story)
 * -------------------------------------------------------
 *   Idle     : PD6 off
 *   Sine     : 1 long blink      — smooth, like a sine shape
 *   Square   : 2 equal pulses    — 50 % duty mimics a square
 *   Triangle : 3 short pulses    — three sides of a triangle
 *   Sawtooth : 4 quick pulses    — four ramp steps
 *
 * Frequency range (PD7 blink speed)
 * -----------------------------------
 *   < 10 Hz  : 1 blink / 2 s   (very slow)
 *   10–100   : 1 blink / s
 *   100–1k   : 2 blinks / s
 *   1k–10k   : 4 blinks / s
 *   > 10k    : rapid flutter
 *
 * Usage
 * -----
 *   led_init();                          // once, before sei()
 *   led_set_state(LS_SINE, 500);         // 500 Hz sine
 *   for (;;) { led_update(); ... }       // main loop, no delay needed
 */

#pragma once

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

extern uint32_t millis(void);   /* defined in millis.c */

/* ── Pin assignments ──────────────────────────────────────────── */
#define LED_SYS_BIT   PD5   /* PORTD */
#define LED_WAVE_BIT  PD6   /* PORTD */
#define LED_FREQ_BIT  PD7   /* PORTD */

/* ── Public state enum ────────────────────────────────────────── */
typedef enum {
    LS_IDLE     = 0,
    LS_SINE     = 1,
    LS_SQUARE   = 2,
    LS_TRIANGLE = 3,
    LS_SAWTOOTH = 4,
    LS_COMM_BT  = 5,   /* HC-05 / HC-06 Bluetooth active   */
    LS_COMM_MCU = 6,   /* inter-ATmega32A comms active      */
} led_state_t;

/* ── Internal: per-channel runtime state ──────────────────────── */
typedef struct {
    const uint16_t *pat;   /* alternating ON/OFF durations in ms */
    uint8_t         len;   /* number of entries (always even)    */
    uint8_t         step;  /* current position in pattern        */
    uint32_t        t;     /* millis() at last transition        */
} _led_ch_t;

static _led_ch_t _ch_sys, _ch_wave, _ch_freq;

/* ── Pattern tables ───────────────────────────────────────────── */
/*   Layout: {on0_ms, off0_ms, on1_ms, off1_ms, ...}              */
/*   Even indices = LED ON, odd indices = LED OFF                  */

/* PB5 — system heartbeat */
static const uint16_t _SYS_IDLE[]  = {200,  800              };  /* single slow beat    */
static const uint16_t _SYS_WAVE[]  = { 40,  100,  40,  820   };  /* double-tap active   */
static const uint16_t _SYS_BT[]    = {150,  150              };  /* rapid equal blink   */
static const uint16_t _SYS_MCU[]   = { 50,   50,  50,  350   };  /* double fast burst   */

/* PD6 — wave type (pulse count = wave type number) */
static const uint16_t _WAVE_SINE[] = {400,  600              };             /* 1 pulse  */
static const uint16_t _WAVE_SQR[]  = {180,  180, 180,  460   };             /* 2 pulses */
static const uint16_t _WAVE_TRI[]  = { 90,  100,  90,  100,  90,  530  };   /* 3 pulses */
static const uint16_t _WAVE_SAW[]  = { 70,   80,  70,   80,  70,   80,
                                        70,  450              };             /* 4 pulses */
static const uint16_t _WAVE_BT[]   = {120,  380              };  /* medium pulse        */
static const uint16_t _WAVE_MCU[]  = { 60,  180,  60,  700   };  /* double flash        */

/* PD7 — frequency range */
static const uint16_t _FREQ_VLOW[] = { 80, 1920              };  /* < 10 Hz             */
static const uint16_t _FREQ_LOW[]  = { 80,  920              };  /* 10 – 100 Hz         */
static const uint16_t _FREQ_MID[]  = { 80,  420              };  /* 100 Hz – 1 kHz      */
static const uint16_t _FREQ_HIGH[] = { 60,  190              };  /* 1 kHz – 10 kHz      */
static const uint16_t _FREQ_VHIGH[]= { 50,   50              };  /* > 10 kHz (flutter)  */
static const uint16_t _FREQ_BT[]   = { 40,  160,  40,  760   };  /* BT: double flash    */
static const uint16_t _FREQ_MCU[]  = {100,  100, 100,  100, 100, 500};/* MCU: triple    */

#define _PLEN(a) ((uint8_t)(sizeof(a)/sizeof((a)[0])))

/* ── Internal helpers ─────────────────────────────────────────── */

static void _ch_off(_led_ch_t *ch, volatile uint8_t *port, uint8_t bit)
{
    ch->pat  = 0;
    ch->len  = 0;
    ch->step = 0;
    *port   &= ~(1 << bit);
}

static void _ch_load(_led_ch_t *ch, const uint16_t *pat, uint8_t len,
                     volatile uint8_t *port, uint8_t bit)
{
    ch->pat  = pat;
    ch->len  = len;
    ch->step = 0;
    ch->t    = millis();
    *port   |= (1 << bit);   /* step 0 is always an ON period */
}

static void _ch_tick(_led_ch_t *ch, volatile uint8_t *port, uint8_t bit)
{
    if (!ch->pat || ch->len == 0) return;
    if ((millis() - ch->t) < ch->pat[ch->step]) return;
    ch->t    = millis();
    ch->step = (uint8_t)((ch->step + 1) % ch->len);
    if (ch->step % 2 == 0) *port |=  (1 << bit);  /* even = ON  */
    else                   *port &= ~(1 << bit);  /* odd  = OFF */
}

static void _load_freq(uint32_t hz)
{
    if      (hz <    10) _ch_load(&_ch_freq, _FREQ_VLOW,  _PLEN(_FREQ_VLOW),  &PORTD, LED_FREQ_BIT);
    else if (hz <   100) _ch_load(&_ch_freq, _FREQ_LOW,   _PLEN(_FREQ_LOW),   &PORTD, LED_FREQ_BIT);
    else if (hz <  1000) _ch_load(&_ch_freq, _FREQ_MID,   _PLEN(_FREQ_MID),   &PORTD, LED_FREQ_BIT);
    else if (hz < 10000) _ch_load(&_ch_freq, _FREQ_HIGH,  _PLEN(_FREQ_HIGH),  &PORTD, LED_FREQ_BIT);
    else                 _ch_load(&_ch_freq, _FREQ_VHIGH, _PLEN(_FREQ_VHIGH), &PORTD, LED_FREQ_BIT);
}

/* ── Public API ───────────────────────────────────────────────── */

/**
 * @brief Blink the system LED (PD5) 3x fast as a power-on "alive" signal.
 *        Pure busy-delay, runs before interrupts — proves the chip booted
 *        and is executing user code. Call first, before millis_init().
 */
void led_boot_signature(void)
{
    DDRD |= (1 << LED_SYS_BIT);
    for (uint8_t i = 0; i < 3; i++) {
        PORTD |=  (1 << LED_SYS_BIT);  _delay_ms(120);
        PORTD &= ~(1 << LED_SYS_BIT);  _delay_ms(120);
    }
    _delay_ms(400);
}

/**
 * @brief Initialise LED GPIO and load idle patterns.
 *        Call once before sei(), after millis_init().
 */
void led_init(void)
{
    DDRD  |=  (1 << LED_SYS_BIT) | (1 << LED_WAVE_BIT) | (1 << LED_FREQ_BIT);
    PORTD &= ~((1 << LED_SYS_BIT) | (1 << LED_WAVE_BIT) | (1 << LED_FREQ_BIT));

    _ch_load(&_ch_sys,  _SYS_IDLE, _PLEN(_SYS_IDLE), &PORTD, LED_SYS_BIT);
    _ch_off (&_ch_wave, &PORTD, LED_WAVE_BIT);
    _ch_off (&_ch_freq, &PORTD, LED_FREQ_BIT);
}

/**
 * @brief Switch to a new system state.
 * @param state    New operating state (see led_state_t).
 * @param freq_hz  Current output frequency in Hz (used for PD7 speed).
 *                 Pass 0 for non-wave states.
 */
void led_set_state(led_state_t state, uint32_t freq_hz)
{
    switch (state) {

        case LS_IDLE:
            _ch_load(&_ch_sys,  _SYS_IDLE, _PLEN(_SYS_IDLE), &PORTD, LED_SYS_BIT);
            _ch_off (&_ch_wave, &PORTD, LED_WAVE_BIT);
            _ch_off (&_ch_freq, &PORTD, LED_FREQ_BIT);
            break;

        case LS_SINE:
            _ch_load(&_ch_sys,  _SYS_WAVE,  _PLEN(_SYS_WAVE),  &PORTD, LED_SYS_BIT);
            _ch_load(&_ch_wave, _WAVE_SINE, _PLEN(_WAVE_SINE),  &PORTD, LED_WAVE_BIT);
            _load_freq(freq_hz);
            break;

        case LS_SQUARE:
            _ch_load(&_ch_sys,  _SYS_WAVE,  _PLEN(_SYS_WAVE),  &PORTD, LED_SYS_BIT);
            _ch_load(&_ch_wave, _WAVE_SQR,  _PLEN(_WAVE_SQR),  &PORTD, LED_WAVE_BIT);
            _load_freq(freq_hz);
            break;

        case LS_TRIANGLE:
            _ch_load(&_ch_sys,  _SYS_WAVE,  _PLEN(_SYS_WAVE),  &PORTD, LED_SYS_BIT);
            _ch_load(&_ch_wave, _WAVE_TRI,  _PLEN(_WAVE_TRI),  &PORTD, LED_WAVE_BIT);
            _load_freq(freq_hz);
            break;

        case LS_SAWTOOTH:
            _ch_load(&_ch_sys,  _SYS_WAVE,  _PLEN(_SYS_WAVE),  &PORTD, LED_SYS_BIT);
            _ch_load(&_ch_wave, _WAVE_SAW,  _PLEN(_WAVE_SAW),  &PORTD, LED_WAVE_BIT);
            _load_freq(freq_hz);
            break;

        case LS_COMM_BT:
            _ch_load(&_ch_sys,  _SYS_BT,   _PLEN(_SYS_BT),   &PORTD, LED_SYS_BIT);
            _ch_load(&_ch_wave, _WAVE_BT,  _PLEN(_WAVE_BT),  &PORTD, LED_WAVE_BIT);
            _ch_load(&_ch_freq, _FREQ_BT,  _PLEN(_FREQ_BT),  &PORTD, LED_FREQ_BIT);
            break;

        case LS_COMM_MCU:
            _ch_load(&_ch_sys,  _SYS_MCU,  _PLEN(_SYS_MCU),  &PORTD, LED_SYS_BIT);
            _ch_load(&_ch_wave, _WAVE_MCU, _PLEN(_WAVE_MCU), &PORTD, LED_WAVE_BIT);
            _ch_load(&_ch_freq, _FREQ_MCU, _PLEN(_FREQ_MCU), &PORTD, LED_FREQ_BIT);
            break;
    }
}

/**
 * @brief Advance all LED channels. Call every main loop iteration.
 *        Takes < 10 CPU cycles when no transition is due.
 */
void led_update(void)
{
    _ch_tick(&_ch_sys,  &PORTD, LED_SYS_BIT);
    _ch_tick(&_ch_wave, &PORTD, LED_WAVE_BIT);
    _ch_tick(&_ch_freq, &PORTD, LED_FREQ_BIT);
}
