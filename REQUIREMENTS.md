# Function Generator + Oscilloscope — Features, Requirements & Verification

The master list of what the instrument must do, and whether the **current design**
(see `output_stage_math.qmd`, `CIRCUIT_CHANGES.md`) actually achieves it. Status:
✅ met · ⚠️ needs a fix/decision · ❓ open.

---

## A. Function generator — required features

| # | Feature | Target | Met? | Notes |
|---|---------|--------|------|-------|
| FG1 | Waveform shapes | sine, square, triangle, sawtooth | ✅ | DDS + `wavetable.h` |
| FG2 | Frequency range | 1 Hz – 20 kHz (10 kHz in BOTH mode) | ✅ | Timer2 DDS, 40 kHz sample |
| FG3 | Signal source resolution | 16-bit | ✅ | signal R-2R DAC (2× 595) |
| FG4 | Amplitude range | up to **±13 V** | ✅ | gain moved *after* CD4051; R25/R30=51k → ±12.75V, no clip (GAP-1/2 resolved) |
| FG5 | Amplitude fine resolution | continuous | ✅ | **digital** wavetable scaling (≥50%) + MCU CD4066 coarse octaves — no pot |
| FG12 | Output fault protection | overcurrent shutdown | ✅ | series R + rail clamps + supply-side sense → MCU mute/fault |
| FG13 | Output phase | in-phase with DAC | ✅ | inverting-unity final stage (Rin=Rf=10k) = 4th inversion → net 0°; don't also flip wavetable |
| FG14 | 2-channel DDS (optional) | offset path → ch2 | ⚪ | needs ±15V routing relay/DG419 + BNC2 buffer (+ opt. ch2 filter); 2ch mode = no offset |
| FG6 | Bipolar DC offset | ±(rail − amplitude) | ✅ | offset R-2R DAC + offset gain stage |
| FG7 | Position modes | centered / full-+ / full-− | ✅ | offset DAC sets `D`; default centered |
| FG8 | Centered-by-default | output symmetric on 0 V | ✅ | single 32 kΩ centering at U7 + software cal |
| FG9 | Output smoothing | switchable LPF for sine | ✅ | Sallen-Key (U6A/B) + CD4051 select |
| FG10 | Output connector | BNC | ✅ | with 50 Ω series (R35) |
| FG11 | Remote control | UART / Bluetooth | ✅ | HC-06, `cmd.h` |

## B. Oscilloscope — required features

| # | Feature | Target | Met? | Notes |
|---|---------|--------|------|-------|
| OS1 | SAR ADC | 16-bit, binary search | ✅ | reuses a 16-bit DAC + comparator + 6N137 |
| OS2 | Fast ADC path | internal 10-bit, ~38 kSPS | ✅ | `adc_int.h`, PA0 |
| OS3 | Negative voltage | ±2.5 V via 2.5 V bias | ✅ | `sar_osc.h` / `adc_int.h` decode |
| OS4 | Input protection | fuse + MOV + clamps | ✅ | `OSCILLOSCOPE_FRONTEND.md` |
| OS5 | Comparator isolation | 6N137 protects PD2 | ✅ | open-loop NE5532-B → 6N137 |
| OS6 | DAC reference for SAR | reuse signal ladder | ✅ | follower taps ladder; OSC-only=16-bit SAR, BOTH=16-bit at low freq else 10-bit ADC |

## C. Operating modes

| Mode | Cmd | FG | Scope | Notes |
|------|-----|----|-------|-------|
| FG only | `m0` | 40 kHz DDS | off | |
| OSC only | `m1` | off | 16-bit SAR (~6 kSPS) | SAR reuses the **signal DAC** (free) |
| BOTH | `m2` | 20 kHz DDS | 16-bit SAR (low freq) / 10-bit ADC (high freq) | low freq → ladder time-shared (minor blip); high freq → internal ADC |

---

## Gaps found in deeper review — all resolved

| Gap | Issue | Resolution |
|-----|-------|-----------|
| GAP-1 🔴→✅ | CD4051 (±8V) clips a ±15V signal | **Reordered:** filter+mux run at ±2.5V; gain moved *after* the mux |
| GAP-2 🟠→✅ | TL071 swings only ±13.5V | Target **±13V**: R25/R30=51k, G_max=5.1 → ±12.75V, hardware-cannot-clip |
| GAP-3 🟠→✅ | Can't drive 50Ω at high V | **High-Z output only** + output fault protection (FG12) |
| GAP-4 🟡→✅ | Low-amplitude resolution loss | Coarse CD4066 octave keeps digital scale ≥50% → ≥15-bit always |
| GAP-5 ❓→✅ | FG/scope DAC sharing | Follower taps ladder; OSC-only 16-bit SAR; BOTH 16-bit@low-freq / 10-bit@high-freq |
| GAP-6 🟢 | Square edge ~2.3µs | Accepted (fine ≤100kHz) |

---

## Verified design equations (reference)

Full derivation in `output_stage_math.qmd`. Key results:
`R_cen = 32k`, `R7=R13=10k`, `R25=R30=60k`, gain/offset banks `150k/75k/37.5k/18.75k`,
`R15=R17=R20=10k`. Rail constraint (firmware): `|offset| + 2.5·G ≤ V_rail`.

## Decisions locked
- Centering = single 32 kΩ resistor (U23 bank removed); errors trimmed by offset DAC in software.
- Fine amplitude/offset = digital (16-bit DACs); analog banks = coarse range only.
- Both 16-bit DACs retained.
