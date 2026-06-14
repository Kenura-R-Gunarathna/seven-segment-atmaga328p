# HARDWARE.md — ATmega32A board reference (as-built)

The authoritative pin/peripheral map for firmware, matched to the **ERC-clean, verified KiCad
netlist**. Direction, boot state, what each pin connects to, the bus/relay/interrupt
architecture, the ISP header, and the LED driver. Keep this in sync with the schematic.

Companion docs (same folder): `FINAL_CIRCUIT.md`, `POWER.md`, `PROTECTION_OUTPUT.md`,
`OSCILLOSCOPE_FRONTEND.md`, `SCOPE_DIGITIZER.md`. Spec/math: `../design/`.

> **MCU:** ATmega32A DIP-40 @ 16 MHz external crystal, logic +5 V. **`main.c` calls driver
> functions only** — every `DDRx`/`PORTx`/timer/SPI register write lives in `src/drivers/*.h`.

---

## 1. Complete pin map (verified against netlist)

Legend — **Dir:** In/Out/Analog · **Init:** boot state firmware sets · **Pull:** external (E) /
internal (I) / none · active-LOW marked `↓`.

### PORTB — DAC SPI + control-bus + CH2 fault
| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PB0 | `PB0_CTRL_SER` | Out | 0 | — | **Control-bus DATA** (bit-bang) → SER of U17/U21/U25/U32 |
| PB1 | `PB1_CTRL_SRCLK` | Out | 0 | — | **Control-bus CLOCK** (bit-bang) → SRCLK of U17/U21/U25/U32 |
| PB2 | `PB2_PROT_FAULT_CH2` ↓ | In | — | E (R127→+5V) | **INT2** — CH2 overcurrent (U30 LM393 open-collector) |
| PB3 | `PB3_DAC16_SIG_RCLK` | Out | 0 | — | **Signal DAC** latch (U10/U11 RCLK) |
| PB4 | `PB4_DAC16_OFF_RCLK` | Out | 0 | — | **Offset DAC** latch (U13/U14 RCLK). ⚠️ Also SPI **/SS** — must stay an output |
| PB5 | `PB5_DAC16_R2R_SER` | Out | 0 | — | **HW-SPI MOSI** → SER of U10 (signal) + U13 (offset) · also **ISP MOSI** (J5.1) |
| PB6 | `PB6_GAIN_RCLK` | Out | 0 | — | **Signal gain-bank** latch (U21 RCLK) · also **ISP MISO** (J5.9) |
| PB7 | `PB7_DAC16_R2R_SRCLK` | Out | 0 | — | **HW-SPI SCK** → SRCLK of U10/U11/U13/U14 · also **ISP SCK** (J5.7) |

### PORTC — control-bus latches + relays
| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PC0 | `PC0_OFFGAIN_RCLK` | Out | 0 | — | **Offset gain-bank** latch (U25 RCLK) |
| PC1 | `PC1_RELAY_CH1_CTRL` | Out | 0 | — | Relay K1 — offset / 2-channel routing (0 = offset mode) |
| PC2 | `PC2_RELAY_OUT1_CTRL` | Out | 0 | — | Relay K2 — CH1 BNC output gate (0 = disconnected) |
| PC3 | `PC3_RELAY_OUT2_CTRL` | Out | 0 | — | Relay K3 — CH2 BNC output gate (0 = disconnected) |
| PC4 | `PC4_RELAY_SCOPE_CTRL` | Out | 0 | — | Relay K4 — scope BNC input gate (0 = disconnected) |
| PC5 | `PC5_MUX_RCLK` | Out | 0 | — | **CD4051 source-mux** select latch (U17 RCLK) |
| PC6 | `PC6_RELAY_REDIR_CTRL` | Out | 0 | — | Relay K5 — shared-pack output redirect (0 = FG, 1 = scope) |
| PC7 | `PC7_LED_RCLK` | Out | 0 | — | **Status-LED 595** latch (U32 RCLK) |

### PORTD — UART + SAR + CH1 fault + spare header J3
| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PD0 | `PD0_UART_RXD` | In | — | — | HC-06 TX → MCU RXD (9600 8N1 U2X) via J1.2 |
| PD1 | `PD1_UART_TXD` | Out | 1 | — | MCU TXD → R148/R149 divider → HC-06 RX (J1.1) |
| PD2 | `PD2_INT0` ↓ | In | — | E (R140 4.7k→+5V) | **INT0** — SAR comparator result (U31 LM393 open-collector). Polled in SAR loop |
| PD3 | `PD3_PROT_FAULT_CH1` ↓ | In | — | E (R116→+5V) | **INT1** — CH1 overcurrent (U29 LM393 open-collector) |
| PD4 | — | In | — | I | **spare** → header **J3.1** |
| PD5 | — | In | — | I | **spare** → header **J3.2** |
| PD6 | — | In | — | I | **spare** → header **J3.3** |
| PD7 | — | In | — | I | **spare** → header **J3.4** |

### PORTA + special
| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PA0 | `PA0_SCOPE_ADC` | Analog | — | none | Internal 10-bit ADC — scope node (also U31 SAR +input), 0–5 V, clamped D15/D16 |
| PA1–PA7 | — | In | — | I | **spare** → header **J4.1–J4.7** |
| 9 | `RESET` ↓ | — | — | E (R1 10k→+5V) | reset: SW1 + **ISP J5.5** |
| 10/11 | VCC/GND | Pwr | — | — | +5 V / GND, 100 nF + 10 µF |
| 12/13 | XTAL2/XTAL1 | — | — | — | Y1 16 MHz + 2×22 pF (C20/C21) |
| 30/31/32 | AVCC/GND/AREF | Pwr | — | — | +5 V via L2 10 µH + C22 / GND / C19+R2 on AREF |

**No truly free MCU pins** — every spare is broken out: PA1–PA7 → **J4**, PD4–PD7 → **J3**.

---

## 2. Default / safe boot state
All relays **de-energized (0)** = safe: **both BNC outputs disconnected, scope disconnected,
FG offset mode, output routed to the FG path.** Set before enabling anything:

| Group | Init |
|-------|------|
| Control-bus SER/SRCLK + all latches (PB3/PB4/PB6/PC0/PC5/PC7) | output, **0** (latch on rising edge) |
| HW-SPI PB5/PB7 + **PB4 (/SS)** | output (PB4 must be output even though it's OFF_RCLK) |
| Relays PC1–PC4, PC6 | output, **0** = de-energized = safe |
| Faults PD3, PB2 | input, **no internal pull** (external pull-ups), INT falling-edge |
| SAR PD2 | input, **no internal pull** (external R140) |
| PA0 | ADC input, no pull |
| Spares PD4–PD7, PA1–PA7 | input + internal pull-up (idle on the headers) |

---

## 3. Bus architecture — two independent buses

**(a) Hardware SPI — both 16-bit R-2R DACs (the fast DDS + SAR hot path)**
- **MOSI = PB5, SCK = PB7**, **MSB-first (DORD = 0)**.
- SER of both DAC chains is on PB5; SRCLK of all four DAC 595s is on PB7 → a shift hits **both**
  chains. Select which DAC updates by pulsing **its** latch: **PB3 = signal (U10/U11)**,
  **PB4 = offset (U13/U14)**. Always shift fresh 16 bits *then* pulse the one latch.
- `/SS` = PB4 must be an output or the SPI core drops to slave mode.

**(b) Bit-bang control bus — the slow CMOS-switch / LED 595s (shared DATA+CLOCK, per-device latch)**
- DATA = PB0, CLOCK = PB1 (shared by all four control 595s).

| Device (595) | Latch | Bits used | Drives |
|--------------|-------|-----------|--------|
| U17 source-mux select | **PC5** | 3 of 8 | CD4051 A/B/C → IN_RAW / IN_FILT / IN_SCOPE |
| U21 signal gain bank | **PB6** | 8 | CD4066 binary gain (de/amp) |
| U25 offset gain bank | **PC0** | 8 | CD4066 binary offset gain |
| U32 status LEDs | **PC7** | 8 | 8 LEDs (see §6) |

> Protocol: shift the target's 8 bits out PB0/PB1 (ripples through the chain, harmless), then
> pulse **only that device's latch**. Others hold until their own latch pulses.

---

## 4. Relay map (`relay_switch.kicad_sch`: BC337 + 1N4148 flyback + 1k base + 10k base-pulldown, +5 V coil)

| Relay | Phys | CTRL | 0 = de-energized (default) | 1 = energized |
|-------|------|------|----------------------------|----------------|
| RelayCh1 | K1 | PC1 | offset mode (offset → summing) | 2ch mode (offset → CH2 buffer) |
| RelayOut1 | K2 | PC2 | CH1 BNC **disconnected** | CH1 connected (→ DDS1_BNC) |
| RelayOut2 | K3 | PC3 | CH2 BNC **disconnected** | CH2 connected (→ DDS2_BNC) |
| RelayScope | K4 | PC4 | scope BNC **disconnected** | scope connected (→ divider → IN_SCOPE) |
| RelayRedir | K5 | PC6 | shared pack → **FG output buffer** | shared pack → **scope digitizer** |

On a latched fault (PROTECT mode) firmware **opens the faulting channel's output relay**
(PC2 / PC3).

---

## 5. Interrupts / fault lines
| Vector | Pin | Source | Edge | Pull |
|--------|-----|--------|------|------|
| INT0 | PD2 | SAR comparator **U31 (LM393, +8 V)** | polled in SAR loop (INT free for a future trigger) | E R140 4.7k |
| INT1 | PD3 | ProtectCh1 overcurrent **U29 (LM393, +5 V)** | **falling** (ISC11:10 = 10) | E R116 |
| INT2 | PB2 | ProtectCh2 overcurrent **U30 (LM393, +5 V)** | **falling** (ISC2 = 0) | E R127 |

```c
// fault init (protect.h)
MCUCR  |=  (1<<ISC11); MCUCR &= ~(1<<ISC10);   // INT1 falling
GICR   &= ~(1<<INT2);  MCUCSR &= ~(1<<ISC2);   // disable INT2 before ISC2; falling
GICR   |=  (1<<INT1) | (1<<INT2);
ISR(INT1_vect){ fault_handle(CH1); }
ISR(INT2_vect){ fault_handle(CH2); }
```
Both fault lines are active-LOW, **externally** pulled up — no internal pull-ups. Each
channel's two LM393 halves (+rail/−rail) wire-OR onto its own line; the two channels are
**separate nets** (per-channel ID, no shared OR).

---

## 6. Status LEDs — 8 × via U32 (74HC595)
LEDs are **not** on GPIO — they hang off a shift register on the control bus.
- **U32 74HC595** (+5 V), SER = PB0, SRCLK = PB1, **latch = PC7 (`PC7_LED_RCLK`)**.
- 8 outputs → 470 Ω (R139–R147) → LEDs D17–D24 → GND. Active-HIGH (1 = lit).
- Write the LED state as one byte, shift on PB0/PB1, pulse PC7.

---

## 7. ISP programming header — J5 (2×5 AVR-ISP)
`Conn_02x05_Odd_Even`, standard 10-pin AVR-ISP / USBasp pinout. Shares the SPI lines with the
595 chains (the 595 inputs are high-Z loads → no contention during programming).

| J5 pin | Signal | Net |
|--------|--------|-----|
| 1 | MOSI | `PB5_DAC16_R2R_SER` |
| 2 | VCC | `+5V` |
| 5 | ~RESET | `RESET` |
| 7 | SCK | `PB7_DAC16_R2R_SRCLK` |
| 9 | MISO | `PB6_GAIN_RCLK` |
| 10 | GND | `GND` |
| 3,4,6,8 | (NC) | no-connect |

**Fresh-chip flash:** a blank ATmega runs on the 1 MHz internal RC → USBasp ISP must be ≤ 250 kHz:
close JP3 on the USBasp and `make program-slow`, then `make fuse` (16 MHz crystal), then JP3 off
+ `make program`. (See `Makefile`.)

---

## 8. Timers (do not double-assign)
| Timer | Owner | Config |
|-------|-------|--------|
| Timer0 | `millis.c` | CTC /64, OCR0 = 249 → 1 kHz tick |
| Timer2 | `dds.h` | CTC /8 → 40 kHz DDS sample ISR (20 kHz in BOTH mode) |
| Timer1 | free | — |

---

## 9. Operating modes & the shared gain/offset/summing pack
One gain stage + summing amp + offset DAC is **time-shared** between FG and scope:
- **Source select:** CD4051 `OutputMux` (latch PC5) — X0 `IN_RAW`, X1 `IN_FILT`, X2 `IN_SCOPE`, X3–X7 GND.
- **Dest select:** RelayRedir (PC6) — FG output buffer (0) or scope digitizer (1).
- Firmware sets source + dest from one mode variable so they never mismatch.

| Mode | Cmd | Signal DAC | Source mux | Redirect | Scope digitizer |
|------|-----|-----------|------------|----------|-----------------|
| FG only | m0 | DDS @ 40 kHz | X0/X1 | FG | — |
| OSC only | m1 | SAR reference | X2 (IN_SCOPE) | scope | **16-bit SAR** (signal DAC + U31 → PD2) |
| BOTH | m2 | DDS @ 20 kHz | per use | per use | 16-bit SAR @ low FG freq, else **internal 10-bit ADC** (PA0) |

**SAR:** signal-DAC buffered output `DACV` → U31(−); scope node (biased 0–5 V) → U31(+);
U31 open-collector → R140 4.7k → +5 V → PD2. Firmware binary-searches the DAC code → 16-bit
sample (OSC mode only — signal DAC is free). Decode: `mv = (raw − mid)·2500/mid`, SAR
mid = 0x8000, ADC mid = 511; account for the inverting output stage in the sign map.
Full detail: `SCOPE_DIGITIZER.md`.

---

## 10. IC supply quick reference
| IC | Supply | Notes |
|----|--------|-------|
| ATmega32A (U6) | +5 V | VCC + AVCC separately decoupled |
| Op-amps TL071 / NE5532 | ±15 V | output stage, buffers, filters |
| 74HC595 (U10/U11/U13/U14/U17/U21/U25/U32) | +5 V | DACs + control/LED latches |
| CD4051 (U18) / CD4066 (U22/U23/U26/U27) | **VDD +5 V, VSS/VEE −8 V** | analog must stay −8…+5 V; scope input pre-attenuated + clamped to +5/−8 V before the mux |
| **LM393 (U31) — SAR comparator** | **+8 V** | inputs span 0–5 V (need CM range to 5 V); OC out → R140 → +5 V |
| LM393 (U29/U30) — protection | +5 V | inputs near ground; OC out → R116/R127 → +5 V |
| Relays (Songle SRD-05) | +5 V coil | BC337 driver + 1N4148 flyback |

*(No 6N137 — the SAR isolator was removed; U31's open-collector to +5 V is already MCU-safe.)*

---

## 11. Firmware init checklist
1. All control/relay/LED pins → outputs, **0** (safe: outputs off, FG mode).
2. Force **PB4 output** (SPI /SS) before SPI init.
3. Init HW-SPI master, **MSB-first**, for both DACs (latch signal = PB3, offset = PB4).
4. Fault inputs (no internal pull), INT1/INT2 **falling-edge**, enable.
5. Timer0 (millis), Timer2 (DDS) per their drivers.
6. UART 9600 8N1 U2X (set URSEL when writing UCSRC — ATmega32A quirk).
7. Spares PD4–PD7 / PA1–PA7 → inputs with pull-ups (they sit on J3/J4).
