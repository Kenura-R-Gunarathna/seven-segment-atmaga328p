# Function Generator + Oscilloscope — FINAL Complete Circuit (ASCII)

ATmega32A · two 16-bit R-2R DACs · fully digital control (no pots/manual switches).
Output ±13V (high-Z), centered-by-default, bipolar offset, dual-rail overcurrent protect.
Math: `output_stage_math.qmd` · changes: `CIRCUIT_CHANGES.md` · features: `REQUIREMENTS.md`.

```
═══════════════════════════════════════════════════════════════════════════════════════
 MASTER BLOCK DIAGRAM
═══════════════════════════════════════════════════════════════════════════════════════

 ATmega32A
   │ PB5/PB7/PB3 (HW SPI)        ┌──────────────► SAR comparator (scope) ──► PD2/INT0
   ▼                             │
 [SIGNAL 16-bit DAC]──►[BUFFER]──┤
  2×74HC595 + R-2R     follower  └─[10k]─►[U7 center+unity]─►[Sallen-Key]─►[CD4051 mux]┐
                                          ▲ −8V via 32k                                 │
                                                                                        ▼
 PA5/PA6/PA7 (bit-bang)                                            [OUTPUT AMP: gain+offset]
   ▼                                                                Rf=51k → ±13V         │
 [OFFSET 16-bit DAC]──►[U9 center+unity]──[offset gain U21]──────────────────┘           │
  2×74HC595 + R-2R      ▲ −8V via 32k                                                     ▼
                                                          [DUAL-RAIL PROTECT]──►[R_out]──► BNC
                                                           Rs1+BC557 / Rs2+BC547           (high-Z)
                                                           → dual LM393 → PD3/INT1

 OSCILLOSCOPE: probe → protect/bias → NE5532-B comparator vs DAC → 6N137 → PD2
═══════════════════════════════════════════════════════════════════════════════════════
```

---

## Stage 1 — Signal DAC → buffer → centering (U7)

```
ATmega                  IC1 74HC595          IC2 74HC595         16-bit R-2R ladder
PB5(MOSI)─DS─────────►14 DS    QH'9├──►14 DS         QH'        (10k series / 22k shunt,
PB7(SCK) ─SRCLK──────►11       Q0-7│   11            Q0-7         VOUT at MSB node)
PB3 ─────RCLK────────►12           │   12                          │
+5V→pin16,pin10(SRCLR)             │  +5V,GND                      │ Voc 0–5V (10k Thév.)
GND→pin8 ; pin13(OE)→GND           │                               ▼
                                                          ┌──────────────────┐
                                                          │  U-BUF (TL071)    │  voltage
                                                  Voc ───►│+                  │  follower
                                                          │      ●────────────┼──► node DACV (clean 0–5V)
                                                       ┌──┤−                  │
                                                       │  └──────────────────┘
                                                       └──────────●  (unity fb)

 node DACV ──┬───────────────────────────────► SAR comparator reference (scope)
             │
             └──[R7a 10k]──┐
                           │   ┌──────────────┐
        −8V ──[R_cen 32k]──┼──►│− U7 (TL071)  │
                           │   │              ●──► V1 = −(Vs−2.5) = ±2.5V  (centered on 0)
              GND ─────────┼──►│+             │
                           │   └──────────────┘
                           └──[R7 10k]──● (feedback)
```
`R7 = 10k` (unity) · `R_cen = 32k` from −8V does the centering (no centering IC).

---

## Stage 2 — Sallen-Key LPF + CD4051 mux  (operates at ±2.5V — safe in ±8V world)

```
 V1 (±2.5V) ──┬─────────────────────────────► CD4051 X0  (RAW / unfiltered path)
              │
              │   ┌─ Sallen-Key (NE5532, ±15V) ─┐
              └──►│ R6 10k─R18 10k, C16/C19 1nF  │──► CD4051 X1  (SMOOTH / filtered)
                  └──────────────────────────────┘
                                                  CD4051 (U18, ±8V rails)
                                          A/B/C select ◄── MCU via shift register
                                          X common (pin3) ──► node SELSIG (±2.5V)
```
Mux only ever sees ±2.5V → no clipping. Select smooth for sine, raw for square/saw.

---

## Stage 3 — Output amp: variable gain + offset summing  (creates the ±13V)

```
                            COARSE GAIN BANK (CD4066 U17, MCU-driven, octaves)
 SELSIG (±2.5V) ──►●──[Rg ∈ 51k/25k/12.75k/10k]──┐      Rg,min 10k → ×5.1
                                                  │
 OFFSET (±2.5V) ──────────────[R_off 10k]─────────┤   (from offset path, Stage 1b)
                                                  │   ┌──────────────┐
                                          GND ────┼──►│+ OUTPUT AMP   │
                                                  └──►│− (TL071)      ●──► VOUT (±13V)
                                                      │              │
                                      [C 22pF]───●────┤              │
                                      [Rf 51k] ──●────┘ (feedback)   │
                                                      └──────────────┘
       fine amplitude & offset = DIGITAL (16-bit DAC wavetable scaling ≥50%)
```
`Rf = 51k`, `Rg,min = 10k` ⇒ G_max 5.1 ⇒ **±12.75V, cannot clip** (rail ±13.5V).
Firmware enforces `|offset| + amplitude ≤ 13V`.

### Stage 1b — Offset DAC path (mirror of signal, bit-banged)

```
PA5─data ┐
PA6─clk  ├─►[OFFSET 16-bit DAC, 2×74HC595]──►[U9: −8V/32k center + R13 10k]──► OFFSET (±2.5V)
PA7─latch┘                                    → [R_off 10k] → output amp (fixed ×5.1)
```
No follower (not shared with scope). **No offset gain bank** — offset is a DC level, so
fine offset is 100% digital (16-bit DAC, 0.19mV steps). U21 removed.

---

## Stage 4 — Dual-rail overcurrent protection + output

```
              +15V
               │
             [Rs1 30Ω]                         ← +rail sense (sourcing)
               ├───────────────► OUTPUT AMP V+
               │
          e ┌ BC557(PNP)                        trip when I·Rs1 > 0.6V (~20mA)
        [Rb 1k]┤b (op-amp side of Rs1)
          c └──[Rc 10k]──GND ──► nodeA ──►│− LM393-A │
                                  +5V─[R1]┴[R2]─GND ─►│+         │ open-collector ┐
                                                       └─────────┘                 │
                                                                                   ├─► PD3
          e ┌ BC547(NPN)                                                           │  (INT1,
        [Rb 1k]┤b (op-amp side of Rs2)            nodeB ──►│+ LM393-B │             │  active-LOW,
          c └──[Rc 10k]──GND                          thr ►│−        │ open-coll ──┘  10k pull-up
               │                                            └─────────┘               to +5V)
             [Rs2 30Ω]                          ← −rail sense (sinking)
               ├───────────────► OUTPUT AMP V−
               │
              −15V

 OUTPUT AMP VOUT ──●──[R_out 100–220Ω 0.5W]──────────────► BNC  (high-Z loads only)
                   │                                    │
            [D+ 1N4148]►|─+15V          GND─|◄[D− 1N4148]┘   (rail clamps: external over-V)
```
PNP catches +output faults, NPN catches −output faults → **both polarities**.
Firmware fault modes (UART command, no switch): `PROTECT` / `WARN` / `OFF`.

---

## Oscilloscope front-end (reuses the signal ladder via node DACV)

```
 PROBE ─[F1 100mA]─[MOV]─[R_in 10k 1W]─┬─[1N4148→+5V]─[1N4148→GND]
                                       │
                              [bias 2.5V: 10k+10k +100nF] ─►●─ probe node (0–5V)
                                                            │
                                            ┌───────────────┤  NE5532-B comparator
                              DACV (ref) ──►│− (pin6)       │  (±15V, open-loop)
                                            │+ (pin5)◄───────┘
                                            └─► OUT ─[820Ω]─[1N4148]─► 6N137 LED
                                                  6N137: VCC+5V, VE→GND,
                                                  Vo ─[4.7k→+5V]─► PD2 (INT0)
 Internal 10-bit ADC: probe node ──► PA0   (fast path / BOTH mode)
```
OSC-only: DACV driven by SAR binary search → 16-bit. BOTH: low FG freq=16-bit SAR
(ladder time-shared), high freq=internal 10-bit ADC.

---

## Power (existing — unchanged)

```
+24V ─[L7815]─►+15V ─[L7808]─►+8V       (cascaded: less heat on the 8V regs)
−24V ─[L7915]─►−15V ─[L7908]─►−8V
+24V ─[LM2596T-5 + L2 33µH + D5 1N5822 + C22 220µF]─►+5V
 each reg: input/output caps + 1N4007 reverse-protection diodes
 ±15V → op-amps · ±8V → CD4051/CD4066 · +5V → ATmega/74HC595/6N137/LM393
```

---

## Final pin map (ATmega32A)

| Pin | Net | Function |
|-----|-----|----------|
| PB5 / PB7 / PB3 | MOSI / SCK / latch | Signal 16-bit DAC (HW SPI) |
| PA5 / PA6 / PA7 | data / clk / latch | Offset 16-bit DAC (bit-bang) |
| PD2 (INT0) | scope comparator | 6N137 SAR result |
| PD3 (INT1) | fault | dual-rail overcurrent (active-LOW) |
| PA0 | internal ADC | fast scope path |
| PD0 / PD1 | UART RX/TX | HC-06 Bluetooth |
| PD5 / PD6 / PD7 | LEDs | status / fault |
| (shift regs) | CD4066/CD4051 select | gain/offset/filter control |

## Final BOM (all tronic.lk / nilabra)

74HC595 ×4 · TL071/TL072 (U7, U9, buffer, output amp, offset amp) · NE5532 (filters) ·
CD4066 ×2 (gain/offset banks) · CD4051 (filter mux) · 6N137 · LM393 (dual, protection) ·
BC557 + BC547 (sense) · 1N4148 / 1N4007 · L78xx/L79xx + LM2596 (power) ·
R-2R: 10k & 22k 1% · misc R/C.

**Removed vs old design:** U23 centering CD4066 + R31–R34 (→ single 32k) · U21 offset gain
bank (→ fixed R_off + digital offset) · separate U16 summer (→ merged into output amp) ·
DAC0808 path · 8-bit binary gain idea (→ octaves + digital fine) · external buffer ICs.
```
