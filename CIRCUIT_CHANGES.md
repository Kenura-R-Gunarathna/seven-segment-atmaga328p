# Function Generator — Final Output-Stage Design (KiCad rework list)

Consolidated, decided design for `function_generator.kicad_sch`. Math in
`output_stage_math.qmd`; features/verification in `REQUIREMENTS.md`.

> **HARD RULES:** keep **both 16-bit R-2R DACs**; **100% digital control** — no manual
> pots/switches, no external controller ICs (see `CLAUDE.md`).

---

## Final signal chain (reordered)

```
SIGNAL R-2R DAC (0–5V, always full 16-bit)
   │
 [Voltage follower]  ── clean low-Z 0–5V ──┬──► SAR comparator (oscilloscope)
   │                                       │
   └──[10k]──► U7  (− input)               (scope tap; high-Z)
              + [32k] from −8V  ──► centering
              R7 = 10k feedback  ──► V1 = −(Vs−2.5) = ±2.5V centered on 0
   │
 Sallen-Key LPF(s)        → ±2.5V   (NE5532, small signal, safe in ±8V world)
   │
 CD4051 mux (smooth/raw)  → ±2.5V   (now safe — only ±2.5V passes)
   │
 COARSE GAIN (CD4066 octaves, MCU-driven) + OFFSET sum   → up to ±13V
   │     Rf = 51k, Rg,min = 10k → G_max = 5.1 → ±12.75V (cannot clip on ±13.5V rail)
   │
 OUTPUT PROTECTION (series R + rail clamps; supply-side current sense → MCU)
   │
 [100–220Ω series] → BNC   (high-impedance loads only)
```

Offset path: offset DAC → U9 (10k + 32k centering, R13=10k) → **fixed** R_off=10k into the
output amp (gain ×5.1 → ±12.75V offset). **No offset gain bank** — offset is a DC level (no
waveform shape), so 100% digital scaling of the 16-bit offset DAC is lossless (0.19mV steps).
**No follower on the offset ladder** (not shared with the scope; current-mode into U9 is fine).
The old U21 offset gain bank is **removed**.

**Offset DAC drive:** bit-banged on 3 spare GPIOs (data/clock/latch) — *not* hardware SPI.
Offset changes infrequently, so bit-bang speed is irrelevant. Hardware SPI (PB5/PB7/PB3)
stays reserved for the signal DAC (DDS + SAR hot path). Pick any 3 free pins (PA-port
spares are convenient).

---

## DAC buffer spec (inside DAC16_R2R sub-sheet)

| Item | Spec |
|------|------|
| Role | unity-gain voltage follower (×1, non-inverting, no resistors) |
| Location | inside DAC16_R2R sub-sheet, at ladder VOUT (buffer-at-source) |
| Part | **TL071** (JFET input — ~30pA bias, ideal for the 10k high-Z ladder) |
| Supply | **±15V** (pin7 +15, pin4 −15) + 100nF/rail decoupling |
| Output | buffered low-Z 0–5V → `VOUT` hier. pin → feeds U7 **and** scope comparator |

Sub-sheet pins: `SER, SRCLK, RCLK` (in) · `+15V, −15V, GND` (power) · `VOUT` (buffered out).
Ladder is 10k / **20k** 1% (true 2:1 — ideal, calibration optional).
Do NOT use NE5532 here: its ~200nA bias drops ~2mV across the 10k source. NE5532 stays on
the Sallen-Key filters (low-Z drive, low noise); TL071/JFET on the high-Z buffer.

## Resistor values (solved — see qmd)

| Ref | Role | Value |
|-----|------|-------|
| R7, R13 | U7/U9 feedback | **10k** |
| R_cen1, R_cen2 | centering from −8V into U7/U9 | **32k** (E96 32.4k) |
| Rf (output amp) | gain feedback | **51k** (→ G_max 5.1, ±12.75V) |
| Gain bank (signal only) | CD4066 octaves | **51k / 25.5k / 12.75k / 10k** (×1/×2/×4/×5.1) |
| R_off | offset → output amp (fixed) | **10k** (offset fine = digital, no bank) |
| R15, R17, R20 | final summing | **10k** |
| R_out | output series | **100–220Ω** |
| Sallen-Key R6/R8/R18/R19 | filter | 10k (unchanged) |

Fine amplitude & offset = **digital** (16-bit DAC wavetable scaling, kept ≥50%).
Coarse range = MCU-driven CD4066. No analog pot, no digital-pot chip.

---

## Summing amp + final output stage (phase handling)

**Summing amp (U16, on main sheet — NOT sub-sheeted, it's a single op-amp):**
- TL071, ±15V, 0.1µF decoupling, 47pF across feedback
- Signal in (V_SIG from GainSignal) via **R17 = 4.7k**; offset in (V_OFFSET from GainOffset)
  via **R20 = 4.7k**; feedback **R15 = 4.7k** → **unity** for both (each path already amplified
  to ±12.75V by its own gain bank). `Vout = −(V_SIG + V_OFFSET)`.
- Rail rule (firmware): **|amplitude| + |offset| ≤ 12.75V**.

**Final stage = inverting unity-gain buffer** (replaces the plain follower):
- TL071, ±15V; **Rin = 10k, Rf = 10k** → ×−1.
- Adds a 4th inversion → the whole chain becomes **net IN-PHASE** with the DAC (3 inversions
  in each path + this one = even). Also buffers the output (low-Z to R_out/BNC).
- **Because phase is corrected in hardware, do NOT also flip the wavetable in firmware**
  (that would double-invert). Symmetric waves don't care; sawtooth direction is set by the table.

Inversion count (reference): signal = CenterSignal(inv)→filter/mux(non-inv)→GainSignal(inv)
→Summing(inv) = 3; offset = CenterOffset(inv)→GainOffset(inv)→Summing(inv) = 3. The inverting
final stage makes it 4 → in-phase. Bipolar offset is inherent (DAC crossing 2.5V + ±15V rails),
unaffected by inversion count — only the firmware sign-map changes.

## Optional future feature — 2-channel DDS (offset path → channel 2)

The offset path (16-bit DAC + CenterOffset + GainOffset) is structurally a second channel.
To make it a switchable 2nd DDS output:
- **Routing disconnect on the offset→summing path** carrying ±12.75V → **relay (MCU-driven)
  or ±15V switch (DG419)** — NOT CD4066/CD4051 (those are ±8V). Closed = offset mode; open = 2ch.
- **Channel-2 output buffer + BNC2** (inverting-unity like ch1), fed from GainOffset.
- Optional **2nd OutputFilter** on the offset path (else ch2 is the raw DAC staircase).
- Modes: 1ch+offset / **2ch DDS (no offset)** / oscilloscope. In 2ch mode both DACs are busy →
  scope uses the internal 10-bit ADC; max freq lower (two phase accumulators in the ISR).

## Output protection block (new) — dual-rail current sense

1. **Series resistor** R_out 100–220Ω (0.5W) — passive fault-current limit + isolation;
   negligible for high-Z loads.
2. **Rail clamp diodes** output→+15V and output→−15V (1N4148/BAV199) — catch
   externally-injected over-voltage.
3. **Dual-rail current sense (catches BOTH output polarities):**
   - **+15V rail:** Rs1 (30Ω) + **BC557 (PNP)** → detects positive-output overcurrent
   - **−15V rail:** Rs2 (30Ω) + **BC547 (NPN)** → detects negative-output overcurrent
   - Each transistor's collector (via 10k to GND) → one half of a **dual LM393**
     (threshold ~0.45V via R1/R2 divider). Both LM393 outputs are open-collector →
     **wire-OR'd** → **PD3 (INT1)**, active-LOW, pull-up 10k to +5V.
   - Trip ≈ 0.6V / Rs = 20mA per rail. Transistors dissipate <10mW (R_c limits Ic to
     ~1.5mA) → no heating. All parts (BC557/BC547/LM393) stocked at tronic.lk/nilabra.
   - **Single +15V sensing alone is INSUFFICIENT** — it misses negative-output shorts;
     hence both rails.
4. TL071 internal short-circuit limit = instant first line.

Signal path stays clean: only the series resistor + reverse-biased clamps; sensing is on
the supply rails (sub-mV signal-dependent droop, rejected by op-amp PSRR — invisible at
high-Z).

### Fault handling — firmware modes (NO physical switch; all-digital)

The fault line (PD3/INT1) is always wired; firmware decides the response, toggled by a
UART/Bluetooth command (keeps the all-digital rule — no manual switch):

| Mode | On fault |
|------|----------|
| `PROTECT` | mute output (open gain CD4066) + fault LED + Bluetooth alert + latch until reset |
| `WARN` | fault LED + Bluetooth alert only, keep running |
| `OFF` | ignore the fault line |

---

## Changes to apply in KiCad (user does this manually)

- Add **voltage follower** (1 spare TL071 section) after the signal ladder; route its
  output to U7 input (via 10k) **and** to the SAR comparator.
- **Move** the Sallen-Key + CD4051 mux to operate at the **±2.5V** level (before the gain),
  and place the **coarse gain + offset summing after the mux**.
- Set R7/R13 → 10k; R25/R30 → **51k**; R15/R17/R20 → 10k; gain/offset banks → octave Rg.
- Add **R_cen1/R_cen2 = 32k** from −8V into U7/U9 inverting nodes.
- **Remove** U23 + R31–R34 (centering bank) and the proposed 2nd CD4066-per-bank.
- Add **output protection**: R_out series, rail clamp diodes, supply-side sense + comparator
  → spare MCU pin (e.g. PD3) for fault input.
- **Remove** leftover PORTC 8-bit DAC nets (old DAC0808 plan); does not affect 16-bit DACs.

> NOTE: Claude will **not** edit the .kicad_sch directly (risk of desync); apply values in
> KiCad and verify visually.

---

## Firmware implications

- Amplitude = pick coarse CD4066 octave so the wavetable scale stays 50–100%, then set the
  fine scale digitally; keeps DAC ≥15-bit and centering valid (symmetric about 0x8000).
- Enforce `|offset| + amplitude ≤ 13V`.
- Mode: OSC-only reuses signal ladder for 16-bit SAR; BOTH uses 16-bit SAR at low FG freq
  (accept minor output blip) else internal 10-bit ADC.
- Output-fault ISR: mute + fault LED + Bluetooth message + latch.
- Per-gain-range offset calibration LUT trims centering/Vos drift.
