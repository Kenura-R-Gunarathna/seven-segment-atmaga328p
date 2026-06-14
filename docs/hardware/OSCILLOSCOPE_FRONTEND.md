# Oscilloscope front-end (as-built)

> **Safety:** no mains. Bench/low-voltage signals only (≤ 30 V), no galvanic isolation.

The scope **reuses the FG's shared gain/summing pack** (input mux + redirect relay) and the
**signal 16-bit DAC** as the SAR reference. The digitizer schematic + decode live in
**`SCOPE_DIGITIZER.md`**; pin map in **`HARDWARE.md`**; output/protection in
**`PROTECTION_OUTPUT.md`**; features in `../design/REQUIREMENTS.md`.

> **As-built note:** the SAR comparator is an **LM393 (U31) on +8 V**, open-collector → R140
> 4.7 k → +5 V → **PD2/INT0**. There is **no 6N137 and no NE5532-B comparator** (earlier drafts) —
> the open-collector output is already MCU-safe 0–5 V logic.

---

## Signal path (BNC → digitizer)
```
 SCOPE_BNC
   │
 [RelayScope K4]  PC4 energized = scope connected   (de-energized = isolated)
   │
 [÷ divider]  e.g. 980k/20k  → pre-attenuate to the ±2.5 V working level
   │
 [clamp to +5 / −8 V]  (D6/D7) — the CD4051 rails, NOT ±15 V
   │
 IN_SCOPE ──► CD4051 mux X2 (U18) ──► shared gain bank ──► summing ──► RelayRedir K5 (PC6)
   │                                                                        │
   │  OSC mode: K5 → scope node, held 0–5 V (gain ≤ ±2.5 V + offset DAC +2.5 V center)
   ▼
 scope node ●──[R138 1k]──► PA0  (internal 10-bit ADC, fast; D15/D16 clamp +5/GND)
            └────────────► U31(+)  [SAR comparator]
```
- **RelayRedir (K5)** keeps the ±13 V FG output off PA0/U31 in FG mode; in OSC mode it routes the
  conditioned 0–5 V scope signal to the digitizer.
- The **offset DAC** re-biases at the U7 summing node so the bipolar scope signal lands at 0–5 V
  only at the digitizer (no separate 2.5 V bias network / no input-side centering amp).

---

## Two ADC paths (off the one scope node)
| Path | Pin | Res | Rate | Use |
|------|-----|-----|------|-----|
| Internal ADC | PA0 | 10-bit | ~38 kSPS | fast shape |
| SAR (signal DAC + U31 LM393) | PD2 | 16-bit | ~6 kSPS | precise / slow |

**SAR:** `DACV` (signal-DAC follower U12) → U31(−); scope node → U31(+); U31 OC → R140 → +5 V →
PD2. Binary-search the DAC code until `DACV` crosses the input. OSC-only (signal DAC free);
in BOTH mode use the 10-bit ADC at high FG freq. Decode:
```
mv = (raw − mid)·2500/mid     internal: mid = 511 · SAR: mid = 0x8000
```
(account for the inverting output stage in the sign map). Full schematic: `SCOPE_DIGITIZER.md`.

---

## UART commands (quick reference — HC-06, 9600 8N1 U2X)
| Cmd | Function | Reply |
|-----|----------|-------|
| `f<hz>` | FG frequency (1–20000) | `OK f1000` |
| `w<0-3>` | waveform 0=sin 1=sqr 2=tri 3=saw | `OK w0` |
| `m0/m1/m2` | FG / OSC / BOTH | `OK m…` |
| `o` / `os<n>` | SAR single / stream n (16-bit) | `V:+1.234V` / `+1234` |
| `oi` / `oi<n>` | internal ADC single / stream (10-bit) | `ADC:+1.234V` / `+1234` |
| `s` | status | `f1000 w0 mfg` |

*(amplitude / offset / relay / fault commands are added as firmware is built — see `src/drivers/cmd.h`.)*

---

## Safety checklist
- [ ] RelayScope (K4/PC4) gates the BNC; de-energized = isolated
- [ ] divider + clamp to **+5 / −8 V** (CD4051 rails) before the mux — never ±15 V
- [ ] PA0 clamp diodes D15/D16 to +5/GND, correct orientation
- [ ] U31 (LM393) on +8 V; R140 4.7 k pull-up to +5 V on PD2
- [ ] signal-DAC 74HC595 bypass caps present
- [ ] fault pull-ups inside both ProtectCh instances (R116 / R127)
- [ ] AVCC + VCC separately decoupled
- [ ] probe never on mains / a non-isolated supply
