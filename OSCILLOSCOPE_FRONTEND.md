# ATmega32A — Function Generator + Dual-Path Oscilloscope
## Complete KiCad Circuit Reference

> **Safety:** No mains connection. Bench/low-voltage signals only (≤ 30V).
> No galvanic isolation in this design.

---

## System Block Diagram

```
                    ┌─────────────────────────────────────────────────┐
                    │                 ATmega32A (U1)                   │
                    │                                                   │
  PROBE ──[PROTECT]─┤─ PA0 ──────────────── Internal 10-bit ADC        │
          [BIAS]    │                        (fast, ~38kSPS)            │
              │     ├─ PD2 ←── 6N137 ←── NE5532-B ←─ BIAS NODE        │
              │     │                         ↑                         │
              │     │         16-bit R-2R DAC (2× 74HC595)             │
              │     ├─ PB3 (latch) ───────────────────── ↑             │
              │     ├─ PB5 (MOSI) ────────────────────── │             │
              │     ├─ PB7 (SCK) ─────────────────────── │             │
              │     │                                                   │
              │     ├─ PC0-PC7 ─── 8-bit R-2R DAC ──► FG OUTPUT       │
              │     │                                                   │
              │     ├─ PD0/PD1 ─── UART (RX/TX)                       │
              │     ├─ PD5/PD6/PD7 ─── LED status                     │
              └─────┤─ NE5532-B (+) (comparator input)                 │
                    └─────────────────────────────────────────────────┘
```

---

## Schematic 1 — Input Protection & Bias

**Net names:** PROBE_TIP, PROBE_FUSED, PROBE_CLAMPED, BIAS_NODE

```
PROBE_TIP
    │
   [F1] 100mA 250V slow-blow
    │
PROBE_FUSED
    │
   [MOV1] S14K275  (across PROBE_FUSED to GND)
    │
   [R1] 10kΩ 1W
    │
PROBE_CLAMPED ──────────────────────────────────────────── to PA0 (U1 pin 40)
    │
   [D1] 1N4148 anode→PROBE_CLAMPED, cathode→+5V    (positive clamp)
   [D2] 1N4148 anode→GND,           cathode→PROBE_CLAMPED (negative clamp)
    │
   [R2] 10kΩ
    │
BIAS_NODE ──────────────────────────────────────────────── to U2 pin 5 (+)
    │
   [R3] 10kΩ    (R2 + R3 form 2.5V divider from +5V to GND)
    │
   GND
    │
   [C1] 100nF    (across R3, i.e. BIAS_NODE to GND — noise filter)


+5V ──[R2=10kΩ]──BIAS_NODE──[R3=10kΩ]──GND
                     │
                    [C1] 100nF
                     │
                    GND
```

**Component values:**

| Ref | Part | Value | Note |
|-----|------|-------|------|
| F1 | Fuse holder + fuse | 100mA 250V slow-blow | 5×20mm |
| MOV1 | Varistor | S14K275 (or V275LA10A) | across L-N of probe |
| R1 | Resistor | 10kΩ 1W | fault current limit |
| D1 | Diode | 1N4148 | cathode to +5V |
| D2 | Diode | 1N4148 | anode to GND |
| R2 | Resistor | 10kΩ 1/4W | bias divider top |
| R3 | Resistor | 10kΩ 1/4W | bias divider bottom |
| C1 | Capacitor | 100nF 25V ceramic | bias filter |

**Net connections from this block:**
- `PROBE_CLAMPED` → U1 PA0 (internal ADC, pin 40)
- `BIAS_NODE` → U2 pin 5 (NE5532-B non-inverting input)

---

## Schematic 2 — NE5532 Comparator (U2, DIP-8)

**NE5532 pinout:**

```
        NE5532 (U2)
       ┌────────────┐
OUT-A  1│            │8  V+  (+15V)
IN−-A  2│            │7  OUT-B ──── to 6N137 drive circuit
IN+-A  3│            │6  IN−-B ──── SAR_VOUT (from 16-bit R-2R DAC)
  V−   4│            │5  IN+-B ──── BIAS_NODE (from input stage)
       └────────────┘
         (pin 4 = −15V)
```

**Connections:**

| Pin | Net | Notes |
|-----|-----|-------|
| 1 (OUT-A) | NC | reserved for future HCPL-7840 diff amp |
| 2 (IN−-A) | NC | reserved |
| 3 (IN+-A) | NC | reserved |
| 4 (V−) | −15V | bypass: 100nF to GND close to pin |
| 5 (IN+-B) | BIAS_NODE | probe signal + 2.5V bias |
| 6 (IN−-B) | SAR_VOUT | 16-bit R-2R DAC output |
| 7 (OUT-B) | CMP_OUT | → 6N137 drive circuit |
| 8 (V+) | +15V | bypass: 100nF to GND close to pin |

**Open-loop operation (no feedback):**
- BIAS_NODE > SAR_VOUT → CMP_OUT = +13V → 6N137 LED ON → PD2 = LOW
- BIAS_NODE < SAR_VOUT → CMP_OUT = −13V → 6N137 LED OFF → PD2 = HIGH

Bypass caps: 100nF ceramic between pin 8 and GND, and between pin 4 and GND.

---

## Schematic 3 — 6N137 Digital Isolation (U3, DIP-8)

**Purpose:** Protects ATmega PD2 from comparator's ±13V swing and probe faults.

```
CMP_OUT (U2 pin 7)
     │
    [R4] 820Ω
     │
    [D3] 1N4148  anode→R4, cathode→U3 pin 2
     │
     │           6N137 (U3)
     │          ┌──────────────────┐
     └── pin 2  │A  (LED anode)    │
                │K  (LED cathode)  │── pin 3 → GND
                │                  │
                │  (isolation gap) │
                │                  │
     +5V ── pin 8 │VCC              │
     GND ── pin 7 │VE (enable)      │
                │Vo (output)        │── pin 6 ──[R5] 4.7kΩ── +5V
                └──────────────────┘                │
                                                    PD2_NET ── U1 PD2 (pin 16)
```

| Pin | Net | Notes |
|-----|-----|-------|
| 2 (anode) | via D3+R4 from CMP_OUT | LED drive input |
| 3 (cathode) | GND | |
| 6 (Vo, output) | PD2_NET | open-collector, pull-up to +5V via R5 |
| 7 (VE, enable) | GND | tie low = always enabled |
| 8 (VCC) | +5V | bypass: 100nF to GND |

| Ref | Part | Value |
|-----|------|-------|
| R4 | Resistor | 820Ω 1/4W |
| D3 | Diode | 1N4148 (blocks −13V from reaching U3 LED) |
| R5 | Resistor | 4.7kΩ 1/4W (pull-up on open-collector output) |

---

## Schematic 4 — 16-bit SAR DAC (U4 + U5, 74HC595 DIP-16)

**Daisy-chain: U4 = high byte (D15–D8), U5 = low byte (D7–D0)**

```
ATmega32A                U4 (74HC595, IC1)              U5 (74HC595, IC2)
                        ┌────────────────┐              ┌────────────────┐
PB5 (MOSI) ─────── DS  1│                │              │                │
PB7 (SCK)  ────── SRCLK 2│                │Q7' ─────── DS 1│              │
PB3 (latch)────── RCLK 3│                │     SRCLK  2│              │
                   GND  4│                │     RCLK   3│              │
                   GND  5│                │       GND  4│              │
                         │   Q0(D8)       │8            │   Q0(D0)     │8
                         │   Q1(D9)       │9            │   Q1(D1)     │9
                         │   Q2(D10)      │10           │   Q2(D2)     │10
                         │   Q3(D11)      │11           │   Q3(D3)     │11
                         │   Q4(D12)      │12           │   Q4(D4)     │12
                         │   Q5(D13)      │13           │   Q5(D5)     │13
                         │   Q6(D14)      │14           │   Q6(D6)     │14
                  SAR_   │   Q7(D15)      │15           │   Q7(D7)     │15
                  VOUT──── Q7      VCC  16│             │ VCC        16│ +5V
                         └────────────────┘             └────────────────┘
```

**74HC595 detailed pin table:**

| Pin | Name | U4 connects to | U5 connects to |
|-----|------|---------------|---------------|
| 1 | QB (Q1) | R-2R node D9 | R-2R node D1 |
| 2 | QC (Q2) | R-2R node D10 | R-2R node D2 |
| 3 | QD (Q3) | R-2R node D11 | R-2R node D3 |
| 4 | QE (Q4) | R-2R node D12 | R-2R node D4 |
| 5 | QF (Q5) | R-2R node D13 | R-2R node D5 |
| 6 | QG (Q6) | R-2R node D14 | R-2R node D6 |
| 7 | QH (Q7) | SAR_VOUT (VOUT) | R-2R node D7 |
| 8 | GND | GND | GND |
| 9 | QH' | U5 DS (pin 14) | NC |
| 10 | /SRCLR | +5V (no clear) | +5V |
| 11 | SRCLK | PB7 | PB7 |
| 12 | RCLK | PB3 | PB3 |
| 13 | /OE | GND (always enabled) | GND |
| 14 | DS | PB5 (MOSI) | U4 QH' (pin 9) |
| 15 | QA (Q0) | R-2R node D8 | R-2R node D0 |
| 16 | VCC | +5V | +5V |

Bypass: 100nF ceramic between pin 16 and pin 8 on each IC.

---

## Schematic 5 — 16-bit R-2R Ladder (SAR DAC)

16 bits → 16 shunt resistors (R = 10kΩ 1%) + 15 series resistors + termination (2R = 20kΩ 1%).

```
                                                      SAR_VOUT (to U2 pin 6)
                                                            │
U4-Q7(D15)──[Ra15=10k]──n15──[Rs14=10k]──n14──...──n1──[Rs0=10k]──n0──[Rterm=20k]──GND
                          │              │               │            │
                        [Rb15=20k]    [Rb14=20k]      [Rb1=20k]   [Rb0=20k]
                          │              │               │            │
                         GND            GND             GND          GND
U4-Q6(D14)──[Ra14=10k]──n14
U4-Q5(D13)──[Ra13=10k]──n13
U4-Q4(D12)──[Ra12=10k]──n12
U4-Q3(D11)──[Ra11=10k]──n11
U4-Q2(D10)──[Ra10=10k]──n10
U4-Q1(D9) ──[Ra9 =10k]──n9
U4-Q0(D8) ──[Ra8 =10k]──n8
U5-Q7(D7) ──[Ra7 =10k]──n7
U5-Q6(D6) ──[Ra6 =10k]──n6
U5-Q5(D5) ──[Ra5 =10k]──n5
U5-Q4(D4) ──[Ra4 =10k]──n4
U5-Q3(D3) ──[Ra3 =10k]──n3
U5-Q2(D2) ──[Ra2 =10k]──n2
U5-Q1(D1) ──[Ra1 =10k]──n1
U5-Q0(D0) ──[Ra0 =10k]──n0
```

**Resistor count:**
- Ra0–Ra15: 16 × 10kΩ 1% (shunt arm — connects bit to node)
- Rs0–Rs14: 15 × 10kΩ 1% (series spine between nodes)
- Rb0–Rb15: 16 × 20kΩ 1% (shunt to GND — one per node)
- Rterm: 1 × 20kΩ 1% (termination at LSB end)
- **Total: 31 × 10kΩ + 17 × 20kΩ**

SAR_VOUT = n15 node = U4-Q7 junction.

---

## Schematic 6 — 8-bit R-2R DAC (Function Generator, PORTC)

Same topology, 8 bits. PC7 = MSB (D7), PC0 = LSB (D0).

```
                                               FG_OUT (DAC output, to BNC/jack)
                                                     │
PC7(D7)──[Rg7=10k]──m7──[Rs6=10k]──m6──...──m1──[Rs0=10k]──m0──[Rgt=20k]──GND
                     │             │               │            │
                   [Rh7=20k]   [Rh6=20k]        [Rh1=20k]   [Rh0=20k]
                     │             │               │            │
                    GND           GND             GND          GND
PC6(D6)──[Rg6=10k]──m6
PC5(D5)──[Rg5=10k]──m5
PC4(D4)──[Rg4=10k]──m4
PC3(D3)──[Rg3=10k]──m3
PC2(D2)──[Rg2=10k]──m2
PC1(D1)──[Rg1=10k]──m1
PC0(D0)──[Rg0=10k]──m0
```

**Resistor count: 8 × 10kΩ + 7 × 10kΩ + 8 × 20kΩ + 1 × 20kΩ = 15 × 10kΩ + 9 × 20kΩ**

FG_OUT is at the m7 node (PC7 / MSB side).

---

## Schematic 7 — ATmega32A (U1) Pin Map

```
                ATmega32A DIP-40
              ┌──────────────────┐
  (XCK/T0)PB0 1│                  │40 PA0 (ADC0) ─── PROBE_CLAMPED (fast ADC)
  (T1)   PB1 2│                  │39 PA1 (ADC1) ─── free
  (INT2) PB2 3│                  │38 PA2 (ADC2) ─── free
  (OC0)  PB3 4│◄── SAR LATCH     │37 PA3 (ADC3) ─── free
  (/SS)  PB4 5│◄── existing latch│36 PA4 (ADC4) ─── free
  (MOSI) PB5 6│◄── SPI MOSI      │35 PA5 (ADC5) ─── free
  (MISO) PB6 7│                  │34 PA6 (ADC6) ─── free
  (SCK)  PB7 8│◄── SPI SCK       │33 PA7 (ADC7) ─── free
        RST  9│                  │32 AREF ─── 100nF to GND
        VCC 10│── +5V            │31 GND
        GND 11│── GND            │30 AVCC ─── +5V (100nF+10µF to GND)
       XTAL2 12│── crystal       │29 PC7 (D7 MSB) ─── FG R-2R
       XTAL1 13│── crystal       │28 PC6 ─────────── FG R-2R
  (RXD) PD0 14│◄── UART RX       │27 PC5 ─────────── FG R-2R
  (TXD) PD1 15│──► UART TX       │26 PC4 ─────────── FG R-2R
  (INT0)PD2 16│◄── PD2_NET (6N137)│25 PC3 ─────────── FG R-2R
  (INT1)PD3 17│                  │24 PC2 ─────────── FG R-2R
  (OC1B)PD4 18│                  │23 PC1 ─────────── FG R-2R
  (OC1A)PD5 19│──► LED_SYS       │22 PC0 (D0 LSB) ─── FG R-2R
  (ICP) PD6 20│──► LED_WAVE      │21 PD7 ──► LED_FREQ
              └──────────────────┘
```

**Pin summary:**

| Pin | Port | Direction | Function |
|-----|------|-----------|----------|
| 40 | PA0 | Input | Internal ADC — PROBE_CLAMPED |
| 39–33 | PA1–PA7 | Free | Available for CD4051 expansion |
| 4 | PB3 | Output | SAR DAC latch (RCLK) |
| 5 | PB4 | Output | Existing 595 latch |
| 6 | PB5 | Output | SPI MOSI |
| 8 | PB7 | Output | SPI SCK |
| 22–29 | PC0–PC7 | Output | 8-bit FG R-2R DAC |
| 14 | PD0 | Input | UART RX |
| 15 | PD1 | Output | UART TX |
| 16 | PD2 | Input | 6N137 comparator output |
| 19 | PD5 | Output | LED_SYS |
| 20 | PD6 | Output | LED_WAVE |
| 21 | PD7 | Output | LED_FREQ |

---

## Schematic 8 — Power Connections

```
+15V rail ──── U2 pin 8 (NE5532 V+)     [bypass 100nF to GND]
−15V rail ──── U2 pin 4 (NE5532 V−)     [bypass 100nF to GND]

+5V rail  ──── U1 pin 10 (ATmega VCC)   [bypass 100nF + 10µF to GND]
          ──── U1 pin 30 (ATmega AVCC)   [bypass 100nF + 10µF to GND]
          ──── U3 pin 8  (6N137 VCC)     [bypass 100nF to GND]
          ──── U4 pin 16 (74HC595 VCC)   [bypass 100nF to GND]
          ──── U5 pin 16 (74HC595 VCC)   [bypass 100nF to GND]
          ──── U4 pin 10 (/SRCLR = +5V, no clear)
          ──── U5 pin 10 (/SRCLR = +5V)
          ──── R5 pull-up (4.7kΩ to 6N137 output)
          ──── D1 clamp cathode

GND ──────── U1 pin 11, U3 pin 3, U3 pin 7
        ──── U4 pin 8, U5 pin 8
        ──── U4 pin 13 (/OE = GND, always enabled)
        ──── U5 pin 13
        ──── D2 clamp anode
        ──── R3 (bias divider bottom)
        ──── C1 (bias filter)
        ──── all R-2R termination resistors (Rterm, Rgt)
        ──── all R-2R shunt resistors (Rb, Rh to GND)
        ──── U1 pin 31, AREF (via 100nF)
```

---

## Complete Parts List

### Oscilloscope Input Stage

| Ref | Part | Value | Qty |
|-----|------|-------|-----|
| F1 | Fuse + holder | 100mA 250V slow-blow, 5×20mm | 1 |
| MOV1 | Varistor | S14K275 (or V275LA10A) | 1 |
| R1 | Resistor | 10kΩ 1W | 1 |
| D1, D2 | Diode | 1N4148 | 2 |
| R2, R3 | Resistor | 10kΩ 1/4W | 2 |
| C1 | Capacitor | 100nF 25V ceramic | 1 |

### NE5532 Comparator

| Ref | Part | Value | Qty |
|-----|------|-------|-----|
| U2 | Op-amp | NE5532 DIP-8 (or 2× TL071 DIP-8) | 1 |
| C2, C3 | Capacitor | 100nF 25V ceramic (bypass U2 ±15V) | 2 |

### 6N137 Isolation

| Ref | Part | Value | Qty |
|-----|------|-------|-----|
| U3 | Optocoupler | 6N137 DIP-8 | 1 |
| R4 | Resistor | 820Ω 1/4W | 1 |
| D3 | Diode | 1N4148 | 1 |
| R5 | Resistor | 4.7kΩ 1/4W | 1 |
| C4 | Capacitor | 100nF 25V ceramic (bypass U3 VCC) | 1 |

### 16-bit SAR DAC

| Ref | Part | Value | Qty |
|-----|------|-------|-----|
| U4, U5 | Shift register | 74HC595 DIP-16 | 2 |
| C5, C6 | Capacitor | 100nF 25V ceramic (bypass U4, U5) | 2 |
| Ra0–Ra15 | Resistor | 10kΩ 1% 1/4W | 16 |
| Rs0–Rs14 | Resistor | 10kΩ 1% 1/4W | 15 |
| Rb0–Rb15 | Resistor | 20kΩ 1% 1/4W | 16 |
| Rterm | Resistor | 20kΩ 1% 1/4W | 1 |

*(Ra + Rs = 31 × 10kΩ 1%, Rb + Rterm = 17 × 20kΩ 1%)*

### 8-bit Function Generator DAC (PORTC R-2R)

| Ref | Part | Value | Qty |
|-----|------|-------|-----|
| Rg0–Rg7 | Resistor | 10kΩ 1% 1/4W | 8 |
| Rs_fg0–Rs_fg6 | Resistor | 10kΩ 1% 1/4W | 7 |
| Rh0–Rh7 | Resistor | 20kΩ 1% 1/4W | 8 |
| Rgt | Resistor | 20kΩ 1% 1/4W | 1 |

*(15 × 10kΩ 1% + 9 × 20kΩ 1%)*

### ATmega32A Decoupling

| Ref | Part | Value | Qty |
|-----|------|-------|-----|
| C7, C8 | Capacitor | 100nF ceramic + 10µF electrolytic (VCC pin 10) | 1+1 |
| C9, C10 | Capacitor | 100nF ceramic + 10µF electrolytic (AVCC pin 30) | 1+1 |
| C11 | Capacitor | 100nF ceramic (AREF pin 32) | 1 |
| Y1 | Crystal | 16MHz | 1 |
| C12, C13 | Capacitor | 22pF ceramic (crystal load caps) | 2 |

---

## UART Commands (quick reference)

| Command | Function | Output format |
|---------|----------|--------------|
| `f<hz>` | Set FG frequency (1–20000 Hz) | `OK f1000` |
| `w<0-3>` | Set waveform 0=sin 1=sqr 2=tri 3=saw | `OK w0` |
| `m0` | FG only mode (Timer2 @ 40kHz) | `OK mfg` |
| `m1` | OSC only mode | `OK mosc` |
| `m2` | Both simultaneously (FG 10kHz max) | `OK mboth` |
| `o` | Single SAR reading (16-bit) | `V: +1.234V` |
| `os<n>` | Stream n SAR samples | `+1234\r\n` per line |
| `oi` | Single internal ADC reading (10-bit) | `ADC: +1.234V` |
| `oi<n>` | Stream n internal ADC samples | `+1234\r\n` per line |
| `s` | Status report | `f1000 w0 mfg` |

---

## ADC Comparison (same probe input, same BIAS_NODE)

| Path | MCU pin | Resolution | Sample rate | Best use |
|------|---------|-----------|-------------|---------|
| Internal ADC | PA0 | 10-bit | ~38kSPS | Fast waveform shape |
| SAR ADC | PD2 (via 6N137) | 16-bit | ~6kSPS | Precise DC / slow signals |

Both decode with 2.5V bias:
```
millivolts = (raw - mid) × V_fullscale / mid
Internal: mid = 511,  V_fullscale = 2500 mV
SAR:      mid = 0x8000, V_fullscale = 2500 mV
```

---

## Safety Checklist

- [ ] F1 fuse installed
- [ ] MOV1 varistor fitted
- [ ] D1/D2 clamp diodes correct orientation
- [ ] NE5532 ±15V bypass caps fitted (both pins)
- [ ] 6N137 VCC bypass cap fitted
- [ ] Both 74HC595 bypass caps fitted
- [ ] U4 pin 10 and U5 pin 10 (/SRCLR) tied to +5V
- [ ] U4 pin 13 and U5 pin 13 (/OE) tied to GND
- [ ] AVCC and VCC both decoupled (separate caps)
- [ ] Probe never connected to mains or isolated supply
