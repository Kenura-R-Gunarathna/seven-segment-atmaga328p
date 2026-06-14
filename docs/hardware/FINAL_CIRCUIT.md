# Function Generator + Oscilloscope — Complete Circuit (as-built, ASCII)

ATmega32A · two **16-bit R-2R DACs (both on hardware SPI)** · fully digital control (no
pots/manual switches). Output ±13 V (high-Z), centered-by-default, bipolar offset, dual-rail
overcurrent protect, 16-bit SAR scope. Matches the ERC-clean netlist.

Pin map: `HARDWARE.md` · power: `POWER.md` · output/protection: `PROTECTION_OUTPUT.md` ·
scope: `OSCILLOSCOPE_FRONTEND.md` + `SCOPE_DIGITIZER.md` · math: `../design/output_stage_math.qmd`
· features: `../design/REQUIREMENTS.md`.

```
═══════════════════════════════════════════════════════════════════════════════════════
 MASTER BLOCK DIAGRAM
═══════════════════════════════════════════════════════════════════════════════════════

 ATmega32A ── HW-SPI (PB5 MOSI / PB7 SCK) shifts BOTH DACs; latch PB3=signal, PB4=offset
   │
   ▼  (signal)                                       ┌─► SAR comparator U31 (LM393) ─► PD2/INT0
 [SIGNAL 16-bit DAC]──►[BUFFER U12]──► DACV ──────────┤
  U10/U11 + R-2R       follower      └─[10k]─►[U7 center]─►[Sallen-Key U16]─►[CD4051 mux U18]┐
                                       ▲ −8V/32k                                             │
   ▼  (offset)                                                                              ▼
 [OFFSET 16-bit DAC]──►[U15 center]──►[offset gain U25/U26/U27/U28]──► K1 ─► summing ◄───[gain U21/U22/U23/U24]
  U13/U14 + R-2R        ▲ −8V/32k       (CD4066 binary)                    │     (CD4066 binary, Rf 51k)
                                                                          ▼
                                              [OUTPUT BUFFER ×−1]──►[DUAL-RAIL PROTECT]──►[R_out]──► BNC
                                               U7/U24 region            BC557 + BC547         (high-Z)
                                                                        → U29/U30 → PD3/INT1 + PB2/INT2

 SCOPE: BNC → K4 → [÷ divider + clamp +5/−8] → IN_SCOPE → mux X2 → gain → summing → K5 redirect
        → scope node (0–5V) → PA0 (10-bit) AND U31(+) vs DACV → SAR → PD2
 CONTROL (bit-bang PB0/PB1): U17 mux(latch PC5) · U21 gain(PB6) · U25 offgain(PC0) · U32 LEDs(PC7)
 ISP: J5 (MOSI PB5 / MISO PB6 / SCK PB7 / RESET / +5 / GND)
═══════════════════════════════════════════════════════════════════════════════════════
```

---

## Stage 1 — Signal DAC → buffer → centering (U7)

```
ATmega          U10 74HC595         U11 74HC595        16-bit R-2R ladder (10k/20k 1%)
PB5(MOSI)─SER──►14 SER  QH'9├──►14 SER       QH'        VOUT at MSB node, 0–5V
PB7(SCK) ─SRCLK►11      Q0-7│   11           Q0-7              │ (10k Thévenin, high-Z)
PB3 ─────RCLK──►12          │   12                            ▼
+5V→16, GND→8, OE(13)→GND, SRCLR(10)→+5V              ┌──────────────────┐
                                              VOUT ──►│+ U12 (TL071)     │ follower
                                                   ┌──┤−   ●─────────────┼──► DACV (clean 0–5V)
                                                   └──────────●           └──────────────────┘
 DACV ──┬─────────────────────────► SAR comparator U31(−)
        └──[R7 10k]──┐  ┌──────────────┐
   −8V ──[32k]───────┼─►│− U7 (TL071)  ●──► V1 = −(Vs−2.5) = ±2.5V (centered on 0)
        GND ─────────┼─►│+             │     R7 10k feedback
                     └──[R7 10k]──● (fb)└──────────────┘
```
Both DACs use the **same SER (PB5) / SCK (PB7)**; you latch the one you mean (PB3 signal / PB4
offset). `DACV` (buffered) feeds the centering amp **and** the SAR comparator.

---

## Stage 2 — Sallen-Key LPF + CD4051 source-mux (runs at ±2.5 V, safe in the ±8 V world)

```
 V1 (±2.5V) ──┬──────────────────────────► CD4051 X0  (IN_RAW)
              │   ┌─ Sallen-Key (NE5532 U16, ±15V) ─┐
              └──►│ R/C low-pass                     │──► CD4051 X1  (IN_FILT)
                  └──────────────────────────────────┘
 scope (÷,clamp) ───────────────────────────► CD4051 X2  (IN_SCOPE)
                          CD4051 U18 (VDD +5, VEE −8)  A/B/C ◄── U17 595 (latch PC5)
                          X common ──► node SELSIG (±2.5V)
```

---

## Stage 3 — Output amp: binary gain + offset summing (creates ±13 V)

```
                 SIGNAL GAIN BANK (CD4066 U22/U23, MCU via U21 595, latch PB6)
 SELSIG ──►●──[Rg ∈ 2.56M…20k binary]──┐    gain = Rf·Σ(bit/Rg),  Rf = R96 51k
                                        │    → crosses ×1 at Rg=51k, up to ~×5
 OFFSET ──►[OFFSET GAIN BANK U26/U27]──► K1 ─┤   (offset path, Stage 1b — Rf = R105 51k)
   (via U25 595, latch PC0)                  │   ┌──────────────┐
                                      GND ───┼──►│+ U24 (TL071)  │
                                             └──►│−  summing    ●──► (→ output buffer)
                                  [Cf 22pF]──●───┤              │
                                  [Rf 51k] ──●───┘ (feedback)   │
                                                 └──────────────┘
       fine amplitude & offset = DIGITAL (16-bit DAC wavetable scaling, kept ≥50%)
```
The **offset path has its own gain bank** (U25/U26/U27/U28) — symmetric with the signal path —
and routes through **relay K1** (offset-summing vs 2-channel). Fine amplitude/offset is the
16-bit DAC scaling; the CD4066 banks are coarse range only. Final stage is an inverting unity
buffer → net **in-phase** with the DAC (don't also flip the wavetable). Detail + protection:
`PROTECTION_OUTPUT.md`.

### Stage 1b — Offset DAC path (mirror of signal, also on HW-SPI)
```
PB5(MOSI)─SER─►U13/U14 74HC595 ─► R-2R ─► [U15: −8V/32k center] ─► OFFSET ─► offset gain ─► K1
PB7(SCK)─SRCLK┘   latch = PB4 (OFF_RCLK)
```
No follower on the offset ladder (not shared with the scope). **Offset DAC is on hardware SPI**,
latched by PB4 — *not* bit-banged.

---

## Stage 4 — Dual-rail overcurrent protect + output  (see `PROTECTION_OUTPUT.md`)
```
 +15V ─[Rs1 30Ω]─► buffer V+ ; BC557(PNP) senses +rail ─► U29/U30 LM393 ─┐
 −15V ─[Rs2 30Ω]─► buffer V− ; BC547(NPN) senses −rail ─► U29/U30 LM393 ─┤ open-collector
 trip ≈ 0.6V/30Ω ≈ 20mA          (each ch wire-ORs its 2 halves)        ├─► CH1 → PD3/INT1
 OUTPUT ──●──[R_out 100–220Ω]──► BNC (high-Z)                            └─► CH2 → PB2/INT2
          ├─[1N4148→+15]  [1N4148→−15]   (external over-V clamps)        (R116 / R127 pull-ups)
```
Two channels → two independent interrupts (per-channel fault ID). Fault response is firmware
(PROTECT / WARN / OFF) — opens that channel's output relay.

---

## Oscilloscope digitizer (reuses the signal ladder via DACV) — see `SCOPE_DIGITIZER.md`
```
 BNC → K4(PC4) → [÷ divider + clamp +5/−8] → IN_SCOPE → mux X2 → gain → summing → K5(PC6)
   OSC: K5 NO → scope node (0–5V):
        ● → [R138 1k] → PA0 (internal 10-bit, D15/D16 clamp +5/GND)
        ● → U31(+) ; DACV → U31(−) ;  U31 OC ──[R140 4.7k → +5V]──► PD2/INT0
```
`U31 = LM393 on +8 V`, open-collector to +5 V → MCU-safe logic. **No 6N137, no NE5532-B
comparator** (removed). OSC-only: signal DAC free → binary-searched as the SAR reference =
16-bit. BOTH: 16-bit SAR at low FG freq, else internal 10-bit ADC.

---

## Status LEDs & ISP
- **8 status LEDs** via **U32 74HC595** (latch PC7) → 470 Ω → D17–D24. Byte-addressed on the
  control bus (PB0/PB1). *(Not GPIO-driven.)*
- **ISP** = **J5** 2×5 AVR-ISP: MOSI PB5 / MISO PB6 / SCK PB7 / RESET / +5 V / GND. Shares the
  SPI lines with the 595s (high-Z loads → no contention).

---

## Power (separate board — see `POWER.md`)
```
 2 isolated PSUs → S-250 flipped → split ±24 (common 0V)
 +24 → LM2596-ADJ buck → +5 (U33), +15 (U34)        −24 → 7915 → −15 ; 7908 → −8
 buck = clean enough (+LC); linear post only where it earns it (op-amp rail).
```
*(Old cascade `+24→15→8` linears burned — see `POWER.md` post-mortem.)*

---

## Final BOM (tronic.lk / nilabra)
74HC595 ×8 (U10/U11/U13/U14/U17/U21/U25/U32) · TL071 ×many (buffers, centering, gain, summing,
output) · NE5532 (filter) · CD4066 ×4 (gain/offset banks) · CD4051 (source mux) ·
**LM393 ×3** (U29/U30 protect, **U31 SAR**) · BC557 + BC547 (sense) · BC337 ×5 (relay drivers) ·
Songle SRD-05 ×5 · 1N4148 / 1N4007 / 1N5822 · L78xx/L79xx + **LM2596-ADJ** (power) ·
R-2R 10k/20k 1% · misc R/C · J1 (HC-06) · J3/J4 (spare headers) · J5 (ISP).

**Removed vs early drafts:** 6N137 SAR isolator (→ LM393 U31 OC to +5 V) · DAC0808/8-bit PORTC
path · bit-banged offset DAC (→ hardware SPI) · input-side CenterSignal1 (→ offset DAC re-bias
at U7) · cascaded ±8 linears (→ see `POWER.md`).
