# Scope Digitizer — redirect + dual-path ADC (ASCII)

Completes the oscilloscope: the conditioned scope signal (after divider → mux X2 → gain →
U7 summing, centered to 0–5 V by the offset DAC) is **redirected by K5** to a **dual digitizer** —
a fast internal 10-bit ADC (PA0) and a precise 16-bit SAR (signal-DAC + comparator → PD2).
Companion (same folder): `OSCILLOSCOPE_FRONTEND.md`, `HARDWARE.md`, `PROTECTION_OUTPUT.md`,
`FINAL_CIRCUIT.md`.

> Centering note: **CenterSignal1 removed.** The +2.5 V re-bias is injected at **U7's summing
> node by the offset DAC** (K1 NC → R6 → U7−) — *after* the gain. Same block does FG offset in
> FG mode (time-shared). The scope signal is bipolar through the whole chain and only becomes
> 0–5 V at U7 out, in OSC mode.

---

## Full path
```
SCOPE_BNC → K4 → [÷50 div + clamp ±(+5/−8)] → IN_SCOPE → mux X2 → gain → U7 summing(+offset bias)
                                                                                      │
                                                                              U7 out (0–5V in OSC)
                                                                                      │
                                                                                  K5 COM
```

## K5 — redirect (protects PA0 from the ±13 V FG output)
```
                          ┌── NC ──[R8]──► U8 (FG buffer) → K2 → DDS1_BNC     (FG mode: K5 de-energized)
 U7 out ───── K5 COM ─────┤
                          └── NO ──●───────► scope digitizer                  (OSC mode: K5 energized)
                                   │
                              (U7 out here is held 0–5 V by gain≤±2.5V + offset +2.5V)
```
- **Move R8 onto K5 NC** (was on the U7-out node). Remove the old `K5 NC → GND`.
- **K5 NO** = the digitizer feed (`SCOPE_COND`).
- FG mode → NO open → **PA0 isolated from ±13 V**. OSC mode → NO live, U7 out is 0–5 V → PA0 safe.

---

## Dual-path digitizer (both hang off K5 NO = `SCOPE_COND`)

### Fast path — internal 10-bit ADC
```
 SCOPE_COND ──[R_s 1k]──●────────────────► PA0  (U6 pin 40)
                        │
                 [D_hi 1N4148] ▲ cathode → +5V
                        ●
                 [D_lo 1N4148] ▼ anode → GND
                        │
                       (0–5V clamp — insurance if mode flips before gain settles)
```

### Precise path — 16-bit SAR (reuses the signal DAC as reference)
```
 SCOPE_COND ──[R_p 1k]──●──► U_SAR (+)            ● same 0–5V clamp as above
                                  │
 DACV (U12 out, net 10) ─────────► U_SAR (−)       ← tap the signal-DAC follower
                                  │
                       U_SAR open-collector out ──●──[R_pu 4.7k]── +5V
                                                  │
                                                  └────────────────► PD2 / INT0  (U6 pin 16)
```
- `U_SAR` = **LM393** on **+8 V / GND** (input common-mode covers 0–5 V on +8 V; +5 V can't reach 5 V).
- Open-collector output **pulled to +5 V** → swings **0–5 V → straight to PD2. No 6N137 needed**
  (nothing is ±13 V once both comparator inputs are 0–5 V).
- Firmware binary-searches the signal-DAC code: PD2 = "DACV above/below scope". Converged code = sample.

---

## Components to add
| Ref | Part | Value / supply | Role |
|-----|------|----------------|------|
| U_SAR | LM393 | +8 V / GND | SAR comparator (1 of 2 halves; 2nd spare) |
| R_pu | 4.7k → +5V | — | open-collector pull-up on PD2 line |
| R_s, R_p | 1k | — | series into PA0 / comparator(+) |
| D_hi, D_lo | 1N4148 ×2 | clamp → +5V / GND | keep digitizer node 0–5 V |
| (tap) | wire | — | U12 out `DACV` (net 10) → U_SAR(−) |

Rewire: **R8 → K5 NC**; **K5 NO → `SCOPE_COND`**; delete **K5 NC→GND**.

---

## IC spec — U_SAR (LM393), per convention
Supply **+8 V (pin 8) / GND (pin 4)**. Open-collector outputs (need pull-up). Logic-level: OC out
pulled to **+5 V** → MCU-safe 0–5 V.

| Pin | Name | Type | Connects |
|-----|------|------|----------|
| 3 | +A | Input | `SCOPE_COND` (via R_p 1k + clamp) |
| 2 | −A | Input | `DACV` (signal-DAC follower, net 10) |
| 1 | outA | Open-collector | → R_pu 4.7k → +5V → **PD2** |
| 8 | V+ | Power | +8 V |
| 4 | V− | Power | GND |
| 5/6/7 | B half | — | spare (tie +in to GND, −in to GND, leave out) |

**Hierarchical sheet:** small — fold into the main sheet or a `scope_adc.kicad_sch` sub-sheet
(hier pins `SCOPE_COND` in, `DACV` in, `PD2` out, `PA0` out; power +5/+8/GND global).

---

## Mode settings (firmware)
| | K5 (PC6) | K1 (PC1) | mux sel | gain | offset DAC | reads |
|---|---|---|---|---|---|---|
| **FG** | NC (off) | per mode | X0 / X1 | FG amplitude | FG offset | — |
| **OSC** | NO (on) | NC (off) | X2 (`IN_SCOPE`) | ≤ ±2.5 V | **+2.5 V center** | PA0 (fast) + SAR→PD2 (precise) |

- OSC: K1 de-energized so the **offset DAC reaches U7** for the +2.5 V centering bias.
- Gain capped so U7 out stays **0–5 V** → PA0 protected, both inputs in LM393 common-mode.
- Signal DAC is free in OSC (mux deselects X0/X1) → it runs the **SAR**, follower = `DACV`.

---

## Decode
```
mv = (raw − mid) · Vfs / mid      internal ADC: mid=511 · SAR: mid=0x8000      Vfs ≈ 2500 mV
```
(SAR is inverting through U7 — account for sign in the decode / flip in firmware.)
