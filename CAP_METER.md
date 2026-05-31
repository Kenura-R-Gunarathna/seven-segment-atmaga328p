# Capacitance meter — circuit notes

Auto-ranging capacitance meter on the ATmega328P. Measures a capacitor by
timing how long it takes to charge through a known resistor (RC step
response), shows it on the 7-seg display, and streams results over Bluetooth.

## Block diagram

```
   Cx ── R(range) ── RC node ──► AVR analog comparator ──► Timer1 capture
            ▲                          ▲
         4067 mux                 ref divider (1/3, 2/3 Vcc)
       (auto-range)                                │
                                                    ▼
   7-seg display ◄── SPI/595        HC-06 ◄── USART (results over Bluetooth)
```

## 1. The RC charging circuit (first-order LOW-pass)

A capacitor charging through a series resistor from a voltage step.

```
  charge step (GPIO HIGH ≈ Vcc) ──[ R ]──┬── node = V_cap  ──► comparator +
                                         │
                                      [ Cx ]   (capacitor under test)
                                         │
                                        GND
```

Step response — the cap charges along an exponential:

```
V_cap(t) = Vcc · ( 1 − e^(−t / RC) )

        Vcc ┤            ____------
            │        _-‾‾
     2/3Vcc ┤     _-‾        ← t_high
            │   _‾
     1/3Vcc ┤ _‾             ← t_low
            │‾
          0 ┼──────────────────► t
```

`τ = RC` is the time constant (time to reach 63.2 % of Vcc). We don't measure
τ directly — we time the crossing of two thresholds (below).

### LPF vs HPF — why this is low-pass
The configuration depends on where the output is taken:

```
 LOW-PASS (this meter)            HIGH-PASS (not used)
   in ──[ R ]──┬── out              in ──┤ C ├──┬── out
              [ C ]                         [ R ]
               │                             │
              GND                           GND
   out across C                     out across R
```

We sense the **capacitor** voltage → **low-pass**: the cap can't change
instantly, so its smooth charging curve is exactly what we time. (Across R it
would be a high-pass / differentiator — a spike that decays — useless here.)

## 2. Two voltage references (Vcc-independent method)

To make the reading independent of supply voltage, time the node between two
thresholds at **1/3 Vcc** and **2/3 Vcc** (the 555-timer levels). Three equal
resistors make both taps:

```
   Vcc
    │
   [R] 10k 1%
    ├────────────► PC3   V_high = 2/3·Vcc
   [R] 10k 1%
    ├────────────► PC4   V_low  = 1/3·Vcc
   [R] 10k 1%
    │
   GND
```

### Switching the reference in firmware
The comparator's **−** input is the ADC multiplexer, so changing reference =
writing the channel number into the low 4 bits of `ADMUX`. Set up once:

```c
ADCSRB |=  (1 << ACME);     // comparator borrows the ADC mux as its - input
ADCSRA &= ~(1 << ADEN);     // ADC must be OFF
ACSR   &= ~(1 << ACBG);     // + input = AIN0/PD6 (the node), NOT the bandgap
```
Then toggle (only the MUX bits change):
```c
#define REF_LOW()   (ADMUX = (ADMUX & 0xF0) | 4)   // ADC4 = PC4 = 1/3 Vcc
#define REF_HIGH()  (ADMUX = (ADMUX & 0xF0) | 3)   // ADC3 = PC3 = 2/3 Vcc
```
`comp_use_adc_pin(4)` / `comp_use_adc_pin(3)` in `comparator.h` do the same.
After switching the mux, wait a few µs before trusting `ACO` / the capture.

The Vcc term cancels and the math collapses to a constant:

```
C = Δt / ( R · ln( (Vcc − Vcc/3) / (Vcc − 2Vcc/3) ) )
  = Δt / ( R · ln 2 )
  = Δt / (0.693 · R)        Δt = t_high − t_low
```

Accuracy depends only on the divider ratio and R — not on Vcc. Use 1 %
resistors for the divider. The references feed the comparator's **−** input,
selected one at a time via the ADC multiplexer (`ADMUX`/`ACME`) — no external
analog switch needed.

## 2b. Phase 1 — single range, no 4067

Get one fixed range working before adding the mux. Just R, Cx, and a charge pin:

```
  PB0 (charge GPIO) ──[ R = 10k ]──┬── node (V_cap) ──► PD6 / AIN0  (comparator +)
                                   │
                                [ Cx ]
                                   │
                                  GND
```

- **PB0 HIGH** charges Cx through R; **PB0 LOW** discharges it (reset between runs).
- **R = 10 kΩ** suits µF electrolytics (τ = RC: 1 µF → 10 ms, 100 µF → 1 s — inside
  Timer1's range). References + 4067 come later.

## 3. Phase 2 — auto-ranging with the CD4067

The 4067 (16:1 analog mux) selects **which range resistor** sits between the
charge pin and the node, so the RC charge time stays in Timer1's good window
across the whole 1 pF – 1 µF span. 4 select lines pick the channel; firmware
steps ranges until the timing lands in range.

### Why auto-ranging is required (math + physics)

We measure C by timing the charge: `Δt = R·C·ln2`, so `C = Δt / (0.693·R)`.
With a **fixed R**, the timer can only see a narrow slice of capacitance — for
two hard reasons:

**1. Math — the timer is bounded at both ends.**
The Timer1 count `N = Δt / t_tick` must satisfy:
```
   N_min  ≤  N  ≤  N_max
   (~200 ticks)   (65535, the 16-bit limit)
```
- *Too few ticks* (small C): quantization error = ±1 tick / N. At N = 5 that's
  ±20 %; you need N ≳ 200 for < 0.5 %.
- *Too many ticks* (large C): N > 65535 **overflows** the 16-bit timer — the
  count wraps and the reading is garbage.

So one R only spans `C ∈ [N_min, N_max] / (0.693·R)` — a ratio of
`65535/200 ≈ 330×`, i.e. **~2.5 decades**. To cover 1 pF → 1 µF (6 decades),
or pF → mF (9 decades), **one resistor physically cannot** — you must change R.
Since `Δt ∝ R·C`, to keep `Δt` in the good window as C varies you scale **R
inversely with C**. C is unknown, so you *try ranges until Δt lands in window* —
that is auto-ranging. A geometric ladder of R (each ~5–10× the last) tiles the
whole span with overlapping windows.

**2. Physics — the RC time constant must match the clock.**
`τ = R·C` sets how fast the capacitor charges:
- **τ too short** (small R·C): the cap charges almost instantly — faster than
  the comparator's propagation delay and the timer's tick. You'd be "timing"
  electrical noise and delay, not the capacitor.
- **τ too long** (large R·C): charging takes seconds to minutes — impractical,
  and it overflows the timer.

Every measuring instrument needs the physical event to unfold on a timescale
its sensor can resolve. Auto-ranging picks R so the charge happens in a
*comfortable* window — long enough to count many ticks (good resolution), short
enough to finish quickly and not overflow. It's the same idea as a **multimeter's
range switch** (200 mV / 2 V / 20 V …) or choosing the right **stopwatch units**
to time a sprinter vs a glacier: match the tool's scale to the quantity, so you
use the full resolution without running off the end.

**The payoff:** more ticks = more counts = lower relative error. Auto-ranging
keeps every measurement near the high-resolution end of the timer, so a 50 pF
cap and a 5 mF cap are *both* measured with the same ~0.5 % timing precision —
something no single fixed resistor could ever do.

### Topology — one resistor per channel, all joining at the node

```
                       CD74HC4067 (16:1 analog mux)
                     ┌───────────────────────────┐
  PB0 (charge) ──────┤ COM/Z                  I0 ├──[ R0  10 Ω  ]──┐
                     │                        I1 ├──[ R1  56 Ω  ]──┤
  S0 ◄── PC0 ────────┤ S0                     I2 ├──[ R2  560 Ω ]──┤
  S1 ◄── PC1 ────────┤ S1                     I3 ├──[ R3  1 k   ]──┤
  S2 ◄── PC2 ────────┤ S2                     .. │      ...        ├── node ─► PD6/AIN0
  S3 ◄── PD7 ────────┤ S3                    I14 ├──[ R14 1 M   ]──┤        │
  EN ◄── GND ────────┤ INH (active-low)      I15 ├──[ R15 5.6 M ]──┘     [ Cx ]
                     └───────────────────────────┘                         │
                                                                          GND
  PB0 HIGH = charge through selected R   |   PB0 LOW = discharge
```

- Each channel `I0..I15` ties through its own resistor to the **common node**
  (the R–Cx junction sensed by AIN0). Unselected channels are high-Z, so only
  the chosen R is in circuit.
- `S0..S3` = binary channel select (0–15). `INH` to GND keeps the mux enabled.
- References (PC3/PC4 divider) and node (PD6) are **unchanged** from Phase 1 —
  the 4067 only swaps the charge resistor.

### Mux on-resistance caveat
The 4067 adds ~70–200 Ω in series (its channel R_on). Negligible for the kΩ–MΩ
ranges, but it **dominates** the 10 Ω / 56 Ω channels. That's fine — each range
is calibrated separately, so R_on folds into that range's constant.

### Range select pins
| Select | Pin |
|---|---|
| S0 | PC0 |
| S1 | PC1 |
| S2 | PC2 |
| S3 | PD7 |
| INH | GND (always enabled) |

```c
// pick channel 0..15
static void mux_select(uint8_t ch) {
    PORTC = (PORTC & ~0x07) | (ch & 0x07);          // S0..S2 = PC0..PC2
    if (ch & 0x08) PORTD |=  (1 << PD7);            // S3 = PD7
    else           PORTD &= ~(1 << PD7);
}
```

### Auto-ranging algorithm
Keep the dt in a healthy window (`DT_MIN ≈ 200`, `DT_MAX ≈ 60000` ticks):

```
 start at a mid channel
 loop:
   dt = measure_once()
   if dt == 0 (timeout) or dt > DT_MAX → cap too big for this R → pick SMALLER R
   else if dt < DT_MIN                  → cap too small for this R → pick LARGER R
   else                                 → in range → done
   stop at channel ends (can't go further)
 report C using THIS channel's calibration {a_ch, b_ch}
```

Larger R → longer dt (use for small caps); smaller R → shorter dt (big caps).

### Resistor bank — measured values (C0–C10 populated)
Use the **measured** R per channel (not nominal) — it makes the per-range
slope exact. C11–C15 free (for the very-small-cap end or finer steps).

```
 ch   R (measured)   Cx range (dt 200..60000 @ 1us/tick)
 C0   10 Ω           ~29 µF  – 8.6 mF
 C1   55.5 Ω         ~5.2 µF – 1.6 mF
 C2   553.5 Ω        ~520 nF – 156 µF
 C3   980 Ω          ~290 nF – 88 µF
 C4   5.519 kΩ       ~52 nF  – 15.7 µF
 C5   9.86 kΩ        ~29 nF  – 8.8 µF
 C6   56.37 kΩ       ~5.1 nF – 1.5 µF
 C7   99.3 kΩ        ~2.9 nF – 872 nF
 C8   560 kΩ         ~515 pF – 155 nF
 C9   1.05 MΩ        ~275 pF – 82 nF
 C10  5.6 MΩ         ~51 pF  – 15.5 nF
 C11..C15  (free)
```
Coverage ≈ **50 pF – 8.6 mF**. The 1–50 pF floor is stray-C limited (~10–20 pF),
not a wiring gap.

### Calibration shortcut (measured R)
Since each channel's R is known, one global constant defines every range:
```
C = dt · K / R_ch + b        K = tick_time / ln_ratio (calibrate once)
                             b ≈ board stray (one number)
a_ch = K / R_ch              per-range slope from measured R — no per-range cap
```
Phase 3 = pin down `K` and `b` with 1–2 known caps, then all ranges follow.

## 4. HC-06 Bluetooth (results out)

The HC-06 is a transparent UART↔Bluetooth bridge; the AVR just writes bytes to
the USART. (Full UART explanation in `UART_NOTES.md`.)

```
   ATmega328P                         HC-06
   PD1 TXD (5V) ──[1k]──┬── RXD       node = 5·2k/(1k+2k) = 3.3V
                       [2k]
                        │
                       GND
   PD0 RXD ◀────────────────── TXD (3.3V, direct)
   GND ─────────────────────── GND   (common ground, mandatory)
```

Results stream as a CSV-ish line per measurement: `RESULT,<ticks>,<ms>`.

## 4b. Clock choice (accuracy & resolution)

The measured time is counted in clock ticks, so the clock sets both the time
**resolution** (finer = better for small caps) and the **accuracy** (a clock
error scales C linearly — the dominant error).

| Option | Resolution | Accuracy | Cost |
|---|---|---|---|
| 1 MHz internal | worst (1 µs/tick) | ±1–10% RC | none |
| **8 MHz internal** (drop CKDIV8) | 8× better | ±1–10% RC (still needs cal, drifts w/ temp) | fuse change only |
| **16 MHz crystal** ⭐ | best (62.5 ns/tick) | ±0.005%, stable | crystal + 2× ~22 pF caps + fuse; gives up PB6/PB7 |

1 MHz and 8 MHz are the **same internal RC** (CKDIV8 just divides) — going
1→8 MHz buys resolution, **not** accuracy. Only an external **crystal** fixes
the dominant clock error and makes calibration trustworthy across temperature.

**Current setting: 8 MHz internal** (`F_CPU=8000000`, `LFUSE=0xE2`). For the
crystal later: `F_CPU=16000000`, `LFUSE=0xFF`, and add load caps.

⚠ **Crystal load caps must be ~18–33 pF** (`C ≈ 2·(CL−C_stray)`). Do **not**
use 0.1 µF / 10 nF (103) caps — they're ~1000× too big and the crystal won't
oscillate.

UART baud auto-derives from `F_CPU` (`UART_UBRR` in `uart.h`), so the clock
switch needs no UART edits — `UBRR` = 12 / 103 / 207 at 1 / 8 / 16 MHz.

## 5. Pin map

| Signal | Pin | Note |
|---|---|---|
| Display SER / SRCLK / RCLK | PB3 / PB5 / PB2 | SPI to 595 chain |
| HC-06 TXD / RXD | PD1 / PD0 | USART (TX via 1k/2k divider) |
| Push button | PD2 | moved off PD1 (=UART TX); INT0 capable |
| Buzzer (active-low) | PD3 | OC2B; LOW = buzz |
| RC node sense | PD6 / AIN0 | comparator + |
| V_high / V_low ref | PC3 / PC4 | comparator − via ADC mux |
| Charge / discharge drive | PB0 | HIGH=charge, LOW=discharge |
| 4067 select S0–S3 | PC0, PC1, PC2, PD7 | range resistors |
| 4067 INH (optional) | PD5 | |

PC5 (old single-reference node) and the internal 1.1 V bandgap are no longer
used — the two divider taps replace them.

## 6. Measurement sequence (firmware)

```
1. discharge:  PB0 = LOW, wait until node ≈ 0
2. ref = V_low   (ADMUX -> PC4)
3. PB0 = HIGH, TCNT1 = 0, arm Timer1 input-capture
4. node crosses V_low  -> capture t_low   (ACO rising edge -> ICR1)
5. ref = V_high  (ADMUX -> PC3), re-arm capture
6. node crosses V_high -> capture t_high
7. Δt = t_high - t_low
8. C  = Δt / (0.693 · R_range);  stream over Bluetooth
```

## 7. Measured results (8 MHz, ps=8 → 1 µs/tick)

### Single-range calibration (locked C7 = 99.3 kΩ)
30-sample trimmed-mean Δt per known cap:

| Cap | Δt (ticks) | in window? |
|---|---|---|
| 100 pF | 12 | ✗ too fast (12 ticks, ±8% quant) |
| 10 nF (mylar) | 733 | ✓ |
| 104 ceramic | 2802 | ✓ (≈40 nF — matches DMM; DC-bias loss) |
| 50 nF | 3579 | ✓ |
| 100 nF | 7788 | ✓ |
| 1 µF | overflow | ✗ too slow |

C7 window ≈ 3–90 nF — confirms the theory. **2-point fit (10 nF, 100 nF):**
```
a = (100000−10000)/(7788−733) = 12.76 pF/tick
b = 10000 − 12.76·733 = +650 pF
CAL_K = a·R = 12.76 × 99300 ≈ 1,266,700      CAL_B = 650
```

### Auto-ranging validation (100 pF → 470 µF)
With auto-ranging on, the meter picked sensible channels across **7 decades**:

| Cap | range picked | reading | note |
|---|---|---|---|
| 0.33 µF | R3 (980 Ω) | 0.337 µF | ✓ |
| 1 µF | R5 (9.86 k) | 0.975 µF | ✓ |
| 2.2 µF | R5 | 2.07 µF | ✓ (electrolytic tol) |
| 10 µF | R4 (5.5 k) | 8.76 µF | −12% (electrolytic tol) |
| 47 µF | R3 | 46.5 µF | ✓ |
| 220 µF | R1 (56 Ω) | **715 µF** | ✗ mux R_on |
| 680 µF | R0 (10 Ω) | overflow | ✗ R_on floor |

**Ranging logic is correct end-to-end.** Accuracy is good wherever R_on is
negligible (R4 and up).

### Mux on-resistance — measured, ~150 Ω
The low-Ω ranges read high because the real path is `R_nominal + R_on`:
```
 from 220 µF on R1:  R_eff = Δt·t_tick/(ln2·C) = 31617µs/(0.693·220µF) ≈ 207 Ω
 R_on ≈ 207 − 56 = ~150 Ω
```
| ch | R_nom | +R_on(150) | error if uncorrected |
|---|---|---|---|
| R0 | 10 Ω | 160 Ω | ~16× (unusable as-is) |
| R1 | 56 Ω | 206 Ω | ~3.7× |
| R2 | 554 Ω | 704 Ω | +27% |
| R3 | 980 Ω | 1130 Ω | +15% |
| R4 | 5.5 k | 5.65 k | +2.7% |
| R5+ | … | … | negligible |

At R7 (99.3 k) R_on is +0.15% — invisible, which is why C7 calibrated cleanly.

### Fixes (Phase 3)
1. Use **effective R** per channel: multimeter the PB0→node resistance with each
   channel selected (= `R_nom + R_on`) and put those in `MUX_R[]`.
2. **Coarser Timer1 prescaler** (e.g. /64 → 8 µs/tick) for the big-cap ranges,
   so caps past the R_on floor (e.g. 680 µF) fit the 16-bit timer.
3. Calibrate `K`, `b` from a multi-range least-squares fit (the simulation).

### Noise
Single-shot Δt scatters ~±15% (comparator has no hysteresis; it jitters at the
slow threshold crossing). The 30-sample **trimmed mean** (drop 3 hi + 3 lo) and
**median** agree closely → the central estimate is stable to a few percent.
