# ATmega32A — Function Generator + Dual-Path Oscilloscope
## KiCad Circuit Reference (oscilloscope front-end)

> **Safety:** No mains. Bench/low-voltage signals only (≤ 30V). No galvanic isolation.

> **Updated for the current design.** Changes vs the old revision:
> - **8-bit PORTC R-2R DAC removed** — PORTC is now relay + control-latch lines (the FG output
>   uses the **16-bit** signal DAC, not an 8-bit PORTC ladder).
> - **No dedicated SAR DAC ladder** — the SAR **reuses the signal 16-bit R-2R DAC** (the
>   `DAC16_R2R` *signal* instance), tapped through its TL071 voltage follower (`DACV`).
> - **Scope input gated by a relay** (`RelayScope`, PC4) right after the BNC — strict I/O control.
> - **Single shared biased input node** feeds both ADC paths.
> - **Pin map corrected** to the real schematic (faults on PD3/PB2, relays on PC1–PC4, etc.).

Companion docs: `PROTECTION_OUTPUT.md` (output buffer, dual-rail protection, relays, fault pins),
`CIRCUIT_CHANGES.md` / `output_stage_math.qmd` (FG output stage), `REQUIREMENTS.md` (feature map).

---

## System Block Diagram

```
                              ┌──────────────────────────────────────────────┐
                              │                ATmega32A (U1)                 │
 SCOPE BNC                    │                                                │
   │                          │                                                │
 [RelayScope]◄── PC4 ─────────┤                                                │
   │ (COM=BNC, NO=front-end)  │                                                │
 [PROTECT + 2.5V BIAS]        │                                                │
   │                          │                                                │
 BIAS_NODE ───────────────────┤─ PA0 ───────────── Internal 10-bit ADC (fast) │
   │                          │                                                │
   └──────────────────────────┤  NE5532-B (+)  (external comparator)           │
                              │       │  compares BIAS_NODE vs DACV            │
   DACV ◄── signal 16-bit DAC │       ▼                                        │
   (DAC16_R2R follower) ──────┤  6N137 ──► PD2 / INT0  (SAR result bit)        │
                              │                                                │
   signal DAC (2× 74HC595):   ├─ PB5 (MOSI/SER) ─┐                             │
                              ├─ PB7 (SCK/SRCLK) ─┼─► DAC16_R2R signal ladder  │
                              ├─ PB3 (SIG latch) ─┘     → follower → DACV       │
                              │                                                │
                              ├─ PD0/PD1 ─ UART (HC-06)                        │
                              └─ PD5/PD6/PD7 ─ LED status                      │
                              └──────────────────────────────────────────────┘
```

The signal DAC does double duty: in **FG modes** it generates the waveform; in **OSC-only**
it is run as the SAR successive-approximation reference. (BOTH mode: SAR at low FG freq with a
minor output blip, else the internal 10-bit ADC — see `REQUIREMENTS.md` OS6.)

---

## Schematic 1 — Scope input: relay gate → protection → 2.5 V bias

One **biased input node** feeds *both* ADC paths (internal ADC + SAR comparator).

```
SCOPE_BNC
    │
 [RelayScope]  COM = BNC, NO → PROBE_TIP   (PC4 energize = scope connected)
    │
PROBE_TIP
    │
   [F1] 100 mA 250 V slow-blow
    │
   [MOV1] S14K275  (PROBE_FUSED → GND)
    │
   [R1] 10 kΩ 1 W   (fault-current limit)
    │
BIAS_NODE ●─────────────────────────────────── PA0 (U1 pin 40, internal ADC)
    │     └──────────────────────────────────── NE5532-B (+) (U2 pin 5)
    │
   [D1] 1N4148  anode→BIAS_NODE, cathode→+5V    (clamp high)
   [D2] 1N4148  anode→GND,       cathode→BIAS_NODE (clamp low)
    │
   +5V ─[R2 10k]─● BIAS_NODE ●─[R3 10k]─ GND      (2.5 V quiescent bias)
                 └─[C1 100nF]─ GND                (noise filter)
```

Quiescent BIAS_NODE = 2.5 V (R2=R3); probe rides on it, clamped to 0–5 V so a ±2.5 V probe
maps to the ADC's 0–5 V window. Decode: `mv = (raw − mid)·2500/mid`.

| Ref | Part | Value | Note |
|-----|------|-------|------|
| RelayScope | Songle SRD-05VDC-SL-C | — | scope input gate, PC4 (see `relay_switch.kicad_sch`) |
| F1 | Fuse + holder | 100 mA 250 V slow-blow, 5×20 mm | |
| MOV1 | Varistor | S14K275 | |
| R1 | Resistor | 10 kΩ 1 W | fault-current limit |
| D1, D2 | Diode | 1N4148 | 0–5 V clamp |
| R2, R3 | Resistor | 10 kΩ 1/4 W | 2.5 V bias divider |
| C1 | Cap | 100 nF | bias filter |

---

## Schematic 2 — NE5532 external comparator (U2, DIP-8, ±15 V)

Open-loop comparator: probe (BIAS_NODE) vs the signal-DAC follower (`DACV`).

```
        NE5532 (U2)            supply ±15V
       ┌────────────┐
OUT-A 1│            │8 V+  ← +15V  (100nF to GND at pin)
IN−-A 2│            │7 OUT-B → CMP_OUT (→ 6N137)
IN+-A 3│            │6 IN−-B ← DACV  (signal 16-bit DAC follower)
  V−  4│            │5 IN+-B ← BIAS_NODE (probe + 2.5V)
       └────────────┘    V− (pin4) ← −15V (100nF to GND at pin)
```

| Pin | Net | Type | Note |
|-----|-----|------|------|
| 1,2,3 (A side) | NC | — | spare half (reserve for future diff amp) |
| 5 IN+-B | BIAS_NODE | Input | probe + 2.5 V |
| 6 IN−-B | DACV | Input | signal-DAC follower output (0–5 V) |
| 7 OUT-B | CMP_OUT | Output | ±13 V swing → 6N137 |
| 8 V+ | +15V | Power | 100 nF bypass |
| 4 V− | −15V | Power | 100 nF bypass |

- BIAS_NODE > DACV → CMP_OUT ≈ +13 V → 6N137 LED ON → PD2 LOW
- BIAS_NODE < DACV → CMP_OUT ≈ −13 V → 6N137 LED OFF → PD2 HIGH

The SAR firmware binary-searches the DAC code until DACV crosses BIAS_NODE.

---

## Schematic 3 — 6N137 isolation (U3, DIP-8) — protects PD2 from ±13 V

```
CMP_OUT (U2 pin7)
   │
  [R4 820Ω]
   │
  [D3 1N4148]  anode→R4, cathode→U3 pin2   (blocks the −13V half)
   │
   ● U3 pin2 (LED anode)
6N137 (U3):
   pin3 (LED cathode) → GND
   pin8 (VCC) → +5V (100nF bypass)
   pin5 (GND) → GND
   pin7 (VE enable) → +5V (enabled)        ── verify per 6N137 variant
   pin6 (Vo, open-collector) → PD2_NET ──[R5 4.7k]── +5V   (pull-up)
                                          └──────── U1 PD2 (pin16, INT0)
```

| Ref | Part | Value |
|-----|------|-------|
| U3 | 6N137 optocoupler | DIP-8 |
| R4 | Resistor | 820 Ω |
| D3 | Diode | 1N4148 (blocks −13 V from the LED) |
| R5 | Resistor | 4.7 kΩ (pull-up on the open-collector output) |
| C4 | Cap | 100 nF (VCC bypass) |

PD2's pull-up lives **here** (R5). The fault lines (PD3/PB2) have their own pull-ups inside
`protection.kicad_sch` — see `PROTECTION_OUTPUT.md`.

---

## Schematic 4 — SAR reference = the signal 16-bit R-2R DAC (no separate SAR DAC)

The SAR does **not** have its own ladder. It reuses the **signal** `DAC16_R2R` instance:

```
PB5 (MOSI/SER) ─┐
PB7 (SCK/SRCLK) ─┼─► 2× 74HC595 ─► R-2R ladder (10k/20k 1%) ─► TL071 follower ─► DACV
PB3 (SIG latch) ─┘                                                              │
                                                          ┌───────────────────┘
                                              ┌───────────┴────────────┐
                                       FG output stage (U7…)    SAR comparator (U2 IN−-B)
```

- Driven over **hardware SPI** (PB5/PB7), latched by **PB3** — the DDS hot path.
- The **TL071 voltage follower** (inside the `DAC16_R2R` sub-sheet) gives a clean low-Z `DACV`
  node shared by the FG output stage **and** the SAR comparator. (A current-mode ladder would
  leave no readable voltage for the comparator — the follower is mandatory here.)
- Ladder = 10 kΩ / 20 kΩ 1 %. Full detail in the `DAC16_R2R` sub-sheet (signal instance);
  the offset DAC is the second instance and is **not** used by the SAR.

There is also a separate **offset** 16-bit DAC (bit-banged: PB0 data / PB1 clock / PB4 latch) —
unrelated to the scope. See `CIRCUIT_CHANGES.md`.

---

## Schematic 5 — ATmega32A (U1) pin map  *(authoritative — matches schematic)*

```
                ATmega32A DIP-40
              ┌──────────────────┐
  PB0 (SER)  1│                  │40 PA0  ── BIAS_NODE (scope internal ADC)
  PB1 (SRCLK)2│                  │39 PA1  ── free
  PB2 (INT2) 3│◄ PROT_FAULT_CH2  │38 PA2  ── free
  PB3        4│► DAC16_SIG_RCLK  │37 PA3  ── free
  PB4        5│► DAC16_OFF_RCLK  │36 PA4  ── free
  PB5 (MOSI) 6│► DAC16_R2R_SER   │35 PA5  ── free
  PB6        7│► GAIN_RCLK       │34 PA6  ── free
  PB7 (SCK)  8│► DAC16_R2R_SRCLK │33 PA7  ── free
  RST        9│                  │32 AREF ── 100nF to GND
  VCC       10│── +5V            │31 GND
  GND       11│── GND            │30 AVCC ── +5V (100nF+10µF)
  XTAL2     12│── 16MHz xtal     │29 PC7  ── free
  XTAL1     13│── 16MHz xtal     │28 PC6  ── free
  PD0 (RXD) 14│◄ UART RX         │27 PC5  ► MUX_RCLK
  PD1 (TXD) 15│► UART TX         │26 PC4  ► RELAY_SCOPE_CTRL
  PD2 (INT0)16│◄ 6N137 (SAR)     │25 PC3  ► RELAY_OUT2_CTRL
  PD3 (INT1)17│◄ PROT_FAULT_CH1  │24 PC2  ► RELAY_OUT1_CTRL
  PD4       18│── free           │23 PC1  ► RELAY_CH1_CTRL
  PD5       19│► LED_SYS         │22 PC0  ► OFFGAIN_RCLK
  PD6       20│► LED_WAVE        │21 PD7  ► LED_FREQ
              └──────────────────┘
```

| Pin | Port | Dir | Net / function |
|-----|------|-----|----------------|
| 1 | PB0 | Out | `PB0_CTRL_SER` — shared bit-bang data (control 595 chain) |
| 2 | PB1 | Out | `PB1_CTRL_SRCLK` — shared bit-bang clock |
| 3 | PB2 | In | `PB2_PROT_FAULT_CH2` (INT2) |
| 4 | PB3 | Out | `PB3_DAC16_SIG_RCLK` — signal DAC latch |
| 5 | PB4 | Out | `PB4_DAC16_OFF_RCLK` — offset DAC latch |
| 6 | PB5 | Out | `PB5_DAC16_R2R_SER` — HW-SPI MOSI → signal DAC |
| 7 | PB6 | Out | `PB6_GAIN_RCLK` — signal gain-bank 595 latch |
| 8 | PB7 | Out | `PB7_DAC16_R2R_SRCLK` — HW-SPI SCK |
| 22 | PC0 | Out | `PC0_OFFGAIN_RCLK` — offset gain-bank 595 latch |
| 23 | PC1 | Out | `PC1_RELAY_CH1_CTRL` — offset/2ch routing relay |
| 24 | PC2 | Out | `PC2_RELAY_OUT1_CTRL` — CH1 BNC relay |
| 25 | PC3 | Out | `PC3_RELAY_OUT2_CTRL` — CH2 BNC relay |
| 26 | PC4 | Out | `PC4_RELAY_SCOPE_CTRL` — scope input relay |
| 27 | PC5 | Out | `PC5_MUX_RCLK` — CD4051 filter-mux 595 latch |
| 28,29 | PC6,PC7 | — | free |
| 14 | PD0 | In | UART RX (HC-06) |
| 15 | PD1 | Out | UART TX |
| 16 | PD2 | In | SAR comparator via 6N137 (INT0) |
| 17 | PD3 | In | `PD3_PROT_FAULT_CH1` (INT1) |
| 18 | PD4 | — | free |
| 19 | PD5 | Out | LED_SYS |
| 20 | PD6 | Out | LED_WAVE |
| 21 | PD7 | Out | LED_FREQ |
| 40 | PA0 | In | BIAS_NODE — scope internal ADC |
| 33–39 | PA1–PA7 | — | free |

**Free pins:** PA1–PA7, PC6, PC7, PD4.

---

## Schematic 6 — Power

```
+15V ─ U2 pin8 (NE5532 V+)        [100nF]
−15V ─ U2 pin4 (NE5532 V−)        [100nF]
+5V  ─ U1 pin10 VCC               [100nF + 10µF]
     ─ U1 pin30 AVCC              [100nF + 10µF]
     ─ U3 pin8 (6N137 VCC)        [100nF]
     ─ signal-DAC 74HC595 VCC ×2  [100nF each]   (DAC16_R2R sub-sheet)
     ─ R5 pull-up (PD2), 6N137 enable, D1 clamp cathode
     ─ relay coils via BC337 drivers (relay_switch sub-sheet)
GND  ─ U1 pin11/31, U3 pins3/5, AREF via 100nF, D2 clamp anode, R3, C1,
       all R-2R termination/shunt resistors
```

---

## UART commands (quick reference)

| Cmd | Function | Reply |
|-----|----------|-------|
| `f<hz>` | FG frequency (1–20000) | `OK f1000` |
| `w<0-3>` | waveform 0=sin 1=sqr 2=tri 3=saw | `OK w0` |
| `m0/m1/m2` | FG / OSC / BOTH | `OK m…` |
| `o` / `os<n>` | SAR single / stream n (16-bit) | `V:+1.234V` / `+1234` |
| `oi` / `oi<n>` | internal ADC single / stream (10-bit) | `ADC:+1.234V` / `+1234` |
| `s` | status | `f1000 w0 mfg` |

*(amplitude / offset / relay / fault commands TBD as firmware is built — see `cmd.h`.)*

---

## ADC paths (both off the one BIAS_NODE)

| Path | Pin | Res | Rate | Use |
|------|-----|-----|------|-----|
| Internal ADC | PA0 | 10-bit | ~38 kSPS | fast shape |
| SAR (signal DAC + comparator + 6N137) | PD2 | 16-bit | ~6 kSPS | precise / slow |

```
mv = (raw − mid)·2500/mid     internal: mid=511 · SAR: mid=0x8000
```

---

## Safety checklist

- [ ] Scope-input relay (RelayScope/PC4) wired COM=BNC, NO=front-end
- [ ] F1 fuse + MOV1 fitted
- [ ] D1/D2 clamp diodes correct orientation (0–5 V on BIAS_NODE)
- [ ] NE5532 ±15 V bypass caps (both pins)
- [ ] 6N137 VCC bypass + R5 pull-up on PD2
- [ ] Signal-DAC 74HC595 bypass caps (DAC16_R2R sheet)
- [ ] Fault pull-ups present inside both ProtectCh instances (R117)
- [ ] AVCC + VCC separately decoupled
- [ ] Probe never on mains / isolated supply
