# HARDWARE.md — ATmega32A board reference for firmware

The authoritative pin/peripheral map for coding. Direction, init/pull state, what each pin
connects to, and the bus/relay/interrupt architecture. Keep this in sync with the KiCad
schematic. Companion: `OSCILLOSCOPE_FRONTEND.md`, `PROTECTION_OUTPUT.md`, `CIRCUIT_CHANGES.md`,
`output_stage_math.qmd`, `REQUIREMENTS.md`.

> **MCU:** ATmega32A DIP-40 @ 16 MHz external crystal. Logic +5V. Convention: `main.c` calls
> driver functions only — all `DDRx`/`PORTx`/timer/SPI register writes live in `src/drivers/*.h`.

---

## 1. Complete pin map

Legend — **Dir:** In/Out/Analog. **Init:** state firmware sets at boot. **Pull:** external (E) /
internal (I) / none. Active-LOW signals marked `↓`.

### PORTB

| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PB0 | `PB0_CTRL_SER` | Out | 0 | — | **Control bus DATA** (bit-bang) → all control 595s (offset DAC, gain, offgain, mux) |
| PB1 | `PB1_CTRL_SRCLK` | Out | 0 | — | **Control bus CLOCK** (bit-bang) → same 595s |
| PB2 | `PB2_PROT_FAULT_CH2` ↓ | In | — | **E** (R117 in ProtectCh2) | **INT2** — ch2 overcurrent (LM393 open-collector) |
| PB3 | `PB3_DAC16_SIG_RCLK` | Out | 0 | — | Signal-DAC 595 **latch** (RCLK) |
| PB4 | `PB4_DAC16_OFF_RCLK` | Out | 0 | — | Offset-DAC 595 **latch**. ⚠️ Also the SPI `/SS` pin — *must stay an output* or SPI drops to slave |
| PB5 | `PB5_DAC16_R2R_SER` | Out | 0 | — | **HW-SPI MOSI** → signal DAC data |
| PB6 | `PB6_GAIN_RCLK` | Out | 0 | — | Signal gain-bank 595 **latch** |
| PB7 | `PB7_DAC16_R2R_SRCLK` | Out | 0 | — | **HW-SPI SCK** → signal DAC clock |

### PORTC

| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PC0 | `PC0_OFFGAIN_RCLK` | Out | 0 | — | Offset gain-bank 595 **latch** |
| PC1 | `PC1_RELAY_CH1_CTRL` | Out | 0 | — | Relay: offset / 2ch routing (0 = offset mode) |
| PC2 | `PC2_RELAY_OUT1_CTRL` | Out | 0 | — | Relay: CH1 BNC output gate (0 = disconnected) |
| PC3 | `PC3_RELAY_OUT2_CTRL` | Out | 0 | — | Relay: CH2 BNC output gate (0 = disconnected) |
| PC4 | `PC4_RELAY_SCOPE_CTRL` | Out | 0 | — | Relay: scope BNC input gate (0 = disconnected) |
| PC5 | `PC5_MUX_RCLK` | Out | 0 | — | CD4051 source-mux select 595 **latch** |
| PC6 | `PC6_RELAY_REDIR_CTRL` | Out | 0 | — | Relay: shared-pack output redirect (0 = FG, 1 = scope) |
| PC7 | — | In | — | I | **free** (pull-up if unused) |

### PORTD

| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PD0 | UART RX | In | — | — | HC-06 TX → MCU (9600 8N1 U2X) |
| PD1 | UART TX | Out | 1 | — | MCU → HC-06 RX |
| PD2 | SAR comparator ↓ | In | — | **E** (R5 4.7k on 6N137) | **INT0** — SAR result bit (6N137 open-collector). Polled in SAR loop |
| PD3 | `PD3_PROT_FAULT_CH1` ↓ | In | — | **E** (R117 in ProtectCh1) | **INT1** — ch1 overcurrent (LM393 open-collector) |
| PD4 | — | In | — | I | **free** |
| PD5 | LED_SYS | Out | 0 | — | Status LED (system) |
| PD6 | LED_WAVE | Out | 0 | — | Status LED (waveform) |
| PD7 | LED_FREQ | Out | 0 | — | Status LED (frequency) |

### PORTA + special

| Pin | Net | Dir | Init | Pull | Connects to / function |
|-----|-----|-----|------|------|------------------------|
| PA0 | scope ADC in | Analog | — | none | Internal 10-bit ADC — shared-pack output (biased 0–5V) |
| PA1–PA7 | — | In | — | I | **free** (analog port; pull-up if unused) |
| 9 | RESET | — | — | E 10k→+5V | ISP reset |
| 10/11 | VCC/GND | Pwr | — | — | +5V / GND, 100nF+10µF decoupling |
| 12/13 | XTAL2/XTAL1 | — | — | — | 16 MHz crystal + 2×22pF |
| 30/31/32 | AVCC/GND/AREF | Pwr | — | — | +5V / GND / 100nF on AREF |

**Free pins:** PC7, PD4, PA1–PA7.

---

## 2. Default / safe boot state

All relays **de-energized (0)** → safe: **both outputs disconnected, scope disconnected,
FG offset mode, output routed to FG path.** Set this before enabling anything:

| Group | Init |
|-------|------|
| Control outputs (SER/SRCLK/all RCLK) | output, **0** (RCLK idle low → latch on rising edge) |
| HW-SPI (PB5/PB7) + **PB4 (/SS)** | output (PB4 must be output even as OFF_RCLK) |
| Relays PC1–PC4, PC6 | output, **0** = de-energized = safe |
| LEDs PD5–PD7 | output, 0 |
| Faults PD3, PB2 | input, **no internal pull** (external R117), INT falling-edge |
| SAR PD2 | input, **no internal pull** (external R5) |
| PA0 | ADC input, no pull |
| Free PC7/PD4/PA1–7 | input + internal pull-up |

---

## 3. Shift-register / DAC bus architecture

**Two independent buses:**

**(a) Hardware SPI — signal DAC only (fast: DDS + SAR hot path)**
- MOSI = PB5, SCK = PB7, latch = PB3.
- Drives the signal 16-bit R-2R DAC (2× 74HC595). **MSB-first (DORD=0).**
- `/SS` = PB4 must be an output (also serves as OFF_RCLK) or the SPI core falls to slave mode.

**(b) Bit-bang control bus — everything slow (shared DATA + CLOCK, per-device LATCH)**
- DATA = PB0, CLOCK = PB1 (shared by all control 595s).
- Per-device latch (pulse only the target's RCLK to update it without disturbing others):

| Device | Latch (RCLK) | Bits | Purpose |
|--------|--------------|------|---------|
| Offset 16-bit DAC (2×595) | **PB4** | 16 | DC offset level |
| Signal gain bank (CD4066 via 595) | **PB6** | 8 | FG/scope amplitude (de/amp) |
| Offset gain bank (CD4066 via 595) | **PC0** | 8 | offset gain |
| Source mux select (CD4051 A/B/C via 595) | **PC5** | 3 (of 8) | IN_RAW / IN_FILT / IN_SCOPE |

> Update protocol on the bit-bang bus: shift the target's pattern out PB0/PB1 (it ripples through
> all chained shift stages, harmless), then **pulse only that device's RCLK** to latch. Outputs of
> the others are untouched until their own RCLK pulses.

---

## 4. Relay map (all via `relay_switch.kicad_sch`: BC337 + 1N4148 flyback + 1k base + 10k pulldown, +5V coil)

| Relay | CTRL | 0 = de-energized (default) | 1 = energized |
|-------|------|----------------------------|----------------|
| RelayCh1 | PC1 | offset mode (COM→NC→summing) | 2ch mode (COM→NO→ch2 buffer) |
| RelayOut1 | PC2 | CH1 BNC **disconnected** | CH1 connected |
| RelayOut2 | PC3 | CH2 BNC **disconnected** | CH2 connected |
| RelayScope | PC4 | scope BNC **disconnected** | scope connected |
| RelayRedir | PC6 | shared pack → **FG output** | shared pack → **scope ADC** |

Fault response (PROTECT mode) **opens the faulting channel's output relay** (PC2/PC3).

---

## 5. Interrupts / fault lines

| Vector | Pin | Source | Edge | Pull |
|--------|-----|--------|------|------|
| INT0 | PD2 | SAR comparator (6N137) | — (polled in SAR loop; INT reserved for future trigger) | E R5 |
| INT1 | PD3 | ProtectCh1 overcurrent | **falling** (ISC11:10 = 10) | E R117 |
| INT2 | PB2 | ProtectCh2 overcurrent | **falling** (ISC2 = 0) | E R117 |

```c
// fault init (in protect.h driver)
MCUCR  |=  (1<<ISC11); MCUCR &= ~(1<<ISC10);   // INT1 falling
GICR   &= ~(1<<INT2);  MCUCSR &= ~(1<<ISC2);   // disable INT2 before ISC2; falling
GICR   |=  (1<<INT1) | (1<<INT2);
ISR(INT1_vect){ fault_handle(CH1); }
ISR(INT2_vect){ fault_handle(CH2); }
```
Both fault lines are active-LOW, externally pulled up (R117 inside each ProtectCh sheet) — **no
internal pull-ups**. Each open-collector pulls its own line; lines are NOT shared.

---

## 6. Timers (do not double-assign)

| Timer | Owner | Config |
|-------|-------|--------|
| Timer0 | `millis.c` | CTC /64, OCR0=249 → 1 kHz tick |
| Timer2 | `dds.h` | CTC /8 → 40 kHz DDS sample ISR (20 kHz in BOTH mode) |
| Timer1 | free | — |

---

## 7. Operating modes & the shared gain/offset/summing pack

One `gain_stage` + summing amp + offset DAC is **time-shared** between FG and scope:
- **Source select:** CD4051 `OutputMux` (latch PC5) — X0 `IN_RAW`, X1 `IN_FILT`, X2 `IN_SCOPE`, X3–X7 GND.
- **Dest select:** `RelayRedir` (PC6) — FG output buffer (0) or scope ADC stage (1).
- Firmware sets source + dest from one mode variable so they never mismatch.

| Mode | Cmd | Signal DAC | Source mux | Redirect | Scope digitizer |
|------|-----|-----------|------------|----------|-----------------|
| FG only | m0 | DDS @ 40 kHz | X0/X1 | FG | — |
| OSC only | m1 | SAR reference | X2 (IN_SCOPE) | scope | **16-bit SAR** (signal DAC + comparator → PD2) |
| BOTH | m2 | DDS @ 20 kHz | per use | per use | 16-bit SAR @ low FG freq, else **internal 10-bit ADC** (PA0) |

**SAR:** signal DAC `VOUT` (buffered follower) → NE5532-B(−); scope input (biased 0–5V) → (+);
6N137 → PD2. Firmware binary-searches the DAC code → 16-bit sample. Only in OSC mode (signal DAC
free). Decode: `mv = (raw − mid)·2500/mid`, SAR mid=0x8000, ADC mid=511.

---

## 8. IC supply quick reference

| IC | Supply | Notes |
|----|--------|-------|
| ATmega32A | +5V | VCC+AVCC separately decoupled |
| Op-amps (TL071/NE5532) | ±15V | output stage, buffers, comparator |
| 74HC595 (all) | +5V | DACs + control latches |
| CD4066 / CD4051 | **VDD +5V, VSS −8V** | analog signal must stay −8…+5V (≈ ±4.5V). Scope input pre-attenuated + clamped to +5/−8V before the mux |
| 6N137 | +5V | open-collector out, R5 4.7k pull-up; enable (VE) **high** |
| LM393 | +5V | open-collector out, R117 pull-up per ProtectCh |
| Relays (Songle SRD-05) | +5V coil | MCU-driven via BC337, 1N4148 flyback |

---

## 9. Firmware init checklist

1. Set all control/relay/LED pins as outputs, init **0** (safe: outputs off, FG mode).
2. Force **PB4 output** (SPI /SS) before SPI init.
3. Init HW-SPI master, **MSB-first**, for the signal DAC.
4. Configure fault inputs (no internal pull), INT1/INT2 falling-edge, enable.
5. Timer0 (millis), Timer2 (DDS) per their drivers.
6. UART 9600 8N1 U2X (URSEL bit when writing UCSRC — ATmega32A quirk).
7. Leave free pins (PC7/PD4/PA1–7) as inputs with pull-ups.
</content>
</invoke>
