# Output Stage — Final Buffer + Dual-Rail Protection + BNC (ASCII)

Final stage of the function generator: inverting-unity output buffer → dual-rail overcurrent
sense → series resistor + rail clamps → BNC (high-Z). Fully digital fault response (no manual
switch). Companion: `FINAL_CIRCUIT.md`, `HARDWARE.md`, `POWER.md` (this folder),
`../design/output_stage_math.qmd`.

```
 Summing Amp out ──► [FINAL BUFFER ×−1] ──► VOUT ──► [PROTECT] ──► [R_out] ──► BNC (high-Z)
                          ▲                              │
                     ±15V via Rs1/Rs2                fault → PD3/INT1
```

---

## Stage 1 — Final buffer (inverting unity, corrects phase + drives output)

```
 from Summing Amp ──[Rin 10k]──●───[Rf 10k]───┐
                               │ (− input)     │
                        GND ──►│+   U_OUT      │
                               │   TL071       ●──────► VOUT  (±12.75V, now IN-PHASE)
                               └───────────────┘
                                V+ (pin7) ← +15V via Rs1
                                V− (pin4) ← −15V via Rs2
                                0.1µF decoupling each rail
```
`Vout = −1 × (summing-amp out)` → 4th inversion → net **in-phase** with the DAC. Unity gain,
buffers the BNC. (Do NOT also flip the wavetable in firmware.)

---

## Stage 2 — Dual-rail overcurrent sense (catches BOTH polarities)

Sense resistors sit in the buffer's **supply** rails (off the signal path). PNP watches the
+rail (sourcing), NPN watches the −rail (sinking).

```
        +15V                                   −15V
         │                                       │
       [Rs1 30Ω]                              [Rs2 30Ω]
         ├─────────► U_OUT V+ (pin7)            ├─────────► U_OUT V− (pin4)
         │                                       │
      e ┌┴┐ BC557 (PNP)                       e ┌┴┐ BC547 (NPN)
   [Rb 1k]┤b ◄ junction of Rs1             [Rb 1k]┤b ◄ junction of Rs2
      c └┬┘                                    c └┬┘
       [Rc 10k]                                 [Rc 10k]
         │                                       │
        GND ──► nodeA                           GND ──► nodeB
                 │                                       │
                 ▼                                       ▼
          ┌─────────────┐                         ┌─────────────┐
   nodeA─►│− LM393-A    │                  nodeB─►│+ LM393-B    │
    Vth ─►│+   (½ chip) │                   Vth ─►│−   (½ chip) │
          └──────┬──────┘                         └──────┬──────┘
                 │  open-collector  ●─────────────────────┘  (wire-OR)
                 └───────────────────●──[10k → +5V]──► PD3 / INT1  (active-LOW fault)

   Vth ≈ 0.45V from a +5V → R1 47k / R2 4.7k divider.
   Trip ≈ 0.6V / Rs = 20mA per rail.  Transistors dissipate <10mW (Rc limits Ic) → cold.
```

PNP catches **positive-output** shorts (op-amp sources), NPN catches **negative-output** shorts
(op-amp sinks). Either pulls PD3 low.

---

## Stage 3 — Output network: series R + rail clamps + BNC

```
 VOUT ──[R_out 100–220Ω, 0.5W]──●─────────────► BNC (high-Z loads only)
                                │
                       [D+ 1N4148] ▲ cathode → +15V   (clamp external over-voltage)
                                │
                               (node)
                                │
                       [D− 1N4148] ▼ anode → −15V     (clamp external under-voltage)
```
- **R_out** limits fault current + isolates cable capacitance (negligible drop into high-Z).
- **Clamps** catch voltage injected from outside the instrument; reverse-biased and silent
  in normal operation.
- TL071 also has **internal short-circuit limiting** (~25mA) = instant first line.

---

## Component table

| Ref | Part / value | Role |
|-----|--------------|------|
| U_OUT | TL071 (±15V) | inverting-unity buffer |
| Rin, Rf | 10k, 10k | buffer ×−1 |
| Rs1, Rs2 | 30Ω 0.25W | rail current sense (trip ≈20mA) |
| BC557 (PNP) | — | +rail fault detect |
| BC547 (NPN) | — | −rail fault detect |
| Rb (×2) | 1k | base limit |
| Rc (×2) | 10k | collector → nodeA/B |
| LM393 | dual comparator (+5V) | both rails → fault |
| R1 / R2 | 47k / 4.7k | Vth ≈0.45V |
| pull-up | 10k → +5V | open-collector fault line |
| R_out | 100–220Ω 0.5W | output series limit |
| D+, D− | 1N4148 | rail clamps |
| decoupling | 0.1µF | each supply pin |

All parts: tronic.lk / nilabra.

---

## IC specs (per convention)

**U_OUT — TL071** · supply ±15V (pin7=+15 via Rs1, pin4=−15 via Rs2)
| Pin | Name | Type | Connects |
|-----|------|------|----------|
| 2 | − | Input | Rin + Rf node |
| 3 | + | Input | GND |
| 6 | out | Output | VOUT → R_out |
| 7 | V+ | Power | +15V (via Rs1) |
| 4 | V− | Power | −15V (via Rs2) |

**LM393 — dual comparator** · supply +5V / GND · open-collector outputs
| Pin grp | Use | Type |
|---------|-----|------|
| A: nodeA vs Vth | +rail trip | Input/Output |
| B: nodeB vs Vth | −rail trip | Input/Output |
| out (wire-OR) | → PD3 | open-collector |

**BC557 (PNP) / BC547 (NPN)** · no supply pins · emitter on the rail, base via Rb to the Rs
junction, collector via Rc to GND.

---

## Firmware fault modes (PD3 / INT1, active-LOW)

Hardware fault line always wired; firmware decides the response (UART command, no switch):

| Mode | On fault (PD3 low) |
|------|--------------------|
| `PROTECT` | mute output (open gain CD4066) + fault LED + Bluetooth alert + latch until reset |
| `WARN` | fault LED + Bluetooth alert only, keep running |
| `OFF` | ignore the fault line |

```c
ISR(INT1_vect) {                 // overcurrent on either rail
    if (fault_mode == OFF) return;
    led_fault_on();
    uart_puts("FAULT: overcurrent\r\n");
    if (fault_mode == PROTECT) { mute_output(); fault_latched = 1; }
}
```

---

## Notes
- Sense is on the **supply rails**, not the signal → the BNC signal path is only a clean
  series resistor + reverse-biased clamps (sub-mV PSRR effect, invisible at high-Z).
- **High-Z output only** (TL071 ~±20mA) — for 50Ω drive you'd add a current booster, not in scope.
- Trip ~20mA is below the TL071's ~25mA internal limit → cuts in before the op-amp is stressed.

---

## Per-channel fault identification — two independent interrupts

`MUX_RCLK` was moved off PB2 (it's just a slow 595 latch strobe → relocated to **PC5**,
`PC5_MUX_RCLK`), freeing **PB2/INT2**. So each channel gets its own external interrupt — clean
identification, no wire-OR, no ID pins.

| Sheet | FAULT → pin | Vector | Net | Pull-up |
|-------|-------------|--------|-----|---------|
| ProtectCh1 | **PD3** | INT1 | `PD3_PROT_FAULT_CH1` | own 10k → +5V |
| ProtectCh2 | **PB2** | INT2 | `PB2_PROT_FAULT_CH2` | own 10k → +5V |

INT0/PD2 = SAR comparator. PB2 = analog-comparator AIN0, but the SAR uses an **external**
comparator → the internal one is unused → PB2 free for INT2. Each line is its own net → **each
needs its own 10k pull-up** (inside each sheet the channel's two LM393 halves +rail/−rail still
wire-OR onto that one FAULT line).

```c
ISR(INT1_vect) { fault_handle(CH1); }   // ProtectCh1
ISR(INT2_vect) { fault_handle(CH2); }   // ProtectCh2
// fault_handle(ch): per-mode response (below) + open THAT channel's output relay
//                   + uart "FAULT CH<n> overcurrent".
```

**ATmega32A interrupt-edge note:** INT2 is **edge-triggered only** → set `ISC2 = 0` (falling
edge) so it fires the instant an active-low fault asserts; the firmware latch holds it after.
Set INT1 to falling edge too (`ISC11:ISC10 = 10`). Edge-trigger is correct for fault onset.

---

## MCU-driven output relays (`relay_switch.kicad_sch`, ×4)

All signal I/O gated by MCU relays (strict I/O control, all-digital). One reusable sub-sheet,
4 instances. **NEVER tie the coil to ±15V — Songle is a 5V coil.**

### Reusable driver block (verified wiring)
```
        +5V ─────┬───────────────┐
                 │               │
           D6 1N4148          K coil (pin5→+5V, pin2→collector)
           cath→+5V              │
                 └──────┬────────┘
                     pin2│ = Q collector (pin1)
                       C │
 CTRL ─[R131 1k]────────B  (Q pin2)   Q = BC337 (NPN)
                        │
                   [R130 10k]          base→GND pulldown (off at boot)
                        │
                      E (pin3) ──┐
                                 │
                               GND  (emitter + pulldown to GND)
```
- Low-side NPN switch. Emitter **direct to GND** (no resistor). 10k is a **base pulldown**, not in
  the emitter. Flyback D6 cathode→+5V. BC337 (~800mA) ≫ Songle coil (~70mA). 1N4007 = sturdier
  flyback alt.
- Hier pins: **CTRL** (in), **COM / NO / NC** (passive). Power via **+5V / GND** globals.
- Verify the symbol's **NO = pin 4 / NC = pin 3** (de-energized rests on NC); swap labels if not.

### Relay instance map

| Instance | CTRL pin | Net | COM | NO (energized) | NC (de-energized, default) |
|----------|----------|-----|-----|----------------|-----------------------------|
| **RelayCh1** (offset / 2ch routing) | **PC1** | `PC1_RELAY_CH1_CTRL` | GainOffset OUT | → U3B ch2 buffer (+) | → R20 4.7k → U16 summing node |
| **RelayOut1** (CH1 BNC) | **PC2** | `PC2_RELAY_OUT1_CTRL` | ch1 output | → BNC1 | (open) |
| **RelayOut2** (CH2 BNC) | **PC3** | `PC3_RELAY_OUT2_CTRL` | ch2 output | → BNC2 | (open) |
| **RelayScope** (scope BNC in) | **PC4** | `PC4_RELAY_SCOPE_CTRL` | scope BNC | → divider→`IN_SCOPE` | (open) |
| **RelayRedir** (output FG↔scope) | **PC6** | `PC6_RELAY_REDIR_CTRL` | shared-pack out | → scope ADC stage (OSC) | → FG output buffer (FG) |

⚠️ **Each relay needs its OWN CTRL net** — do not share one net across relays (they'd all switch
together). PC1–PC4 + PC6, one per relay.

Relays on **PC1–PC4 + PC6**; PA stays the analog/ADC port. `MUX_RCLK` on **PC5**; faults on
**PD3/INT1** + **PB2/INT2**. **PC7, PA1–PA7, PD4 spare.** (Schematic pin map: PB0/PB1 = control-595
SER/SRCLK; PB3/PB4/PB6/PC0/PC5 = per-chip RCLK latches; PB5/PB7 = signal-DAC hardware SPI;
PD2=SAR/INT0, PD5–7=LEDs, PD0/1=UART.)

### Shared gain/offset/summing pack — reused by FG *and* scope (time-shared)
One `gain_stage` + summing amp + offset DAC serves both paths, selected by an input mux and an
output redirect relay:
```
FG raw/filt ─┐
             ├─[CD4051 OutputMux]─► [de/amp gain] ─► [summing + offset] ─► [RelayRedir PC6]─┬─► FG output
scope div ───┘  X0=IN_RAW X1=IN_FILT                                                        └─► scope ADC
   IN_SCOPE→X2  (CD4051 +5V/−8V; IN_SCOPE pre-attenuated ÷ + clamped to +5/−8V before X2)
```
- **Input select:** CD4051 `OutputMux` — X0 `IN_RAW`, X1 `IN_FILT`, X2 `IN_SCOPE`, X3–X7 → GND. Select
  bits driven by a 595 (`PC5_MUX_RCLK`).
- **Output redirect:** `RelayRedir` (PC6) — chain output → FG buffer (FG mode) or scope ADC (OSC mode).
  Must be a **relay** (chain out is ±13V, past the CMOS-switch rails).
- **Firmware lockstep:** the mode bit sets the CD4051 select *and* RelayRedir together so source/dest
  never mismatch. OSC-only mode also frees the offset DAC to cancel the scope's input DC.
- **Scope input front-end:** BNC → RelayScope(PC4) → divider (e.g. 980k/20k) → clamp **to +5V/−8V**
  (the CD4051 rails, NOT ±15V) → `IN_SCOPE` → X2.

### RelayCh1 — offset vs 2-channel routing (Form C / SPDT)
```
                          ┌─ NO(4) ─► U3B ch2 buffer (+) ──[R129 100k]──GND
GainOffset OUT ── COM(1) ─┤
                          └─ NC(3) ─► R20 4.7k ─► U16 summing node ──[R128 100k]──GND
```
| RelayCh1 (PC1) | Contact | Mode |
|----------------|---------|------|
| **de-energized (default)** | COM→NC | **offset mode** — offset sums into ch1 |
| **energized** | COM→NO | **2ch DDS mode** — offset path → ch2 output |

- Summing node lands on a relay **throw** → de-/re-route gives a clean open, no floating-R into
  the virtual ground.
- **R129 (ch2-buffer side) is mandatory** — a non-inverting (+) input floats/rails when its throw
  opens. **R128 (summing side) is redundant-but-harmless** — that node is a virtual ground held by
  feedback; keeping both is fine (symmetry, ~µA load).
- Relay contact gives GΩ isolation when open (far better than a CD4066), and carries ±15V signal
  fine — that's why a relay (not a CD4066/CD4051) is used on this ±12.75V path.

### Fault → output relays
On a latched fault (`PROTECT` mode), firmware **opens that channel's output relay**
(RelayOut1/RelayOut2) in addition to muting the gain bank — a hard physical disconnect of the BNC.
