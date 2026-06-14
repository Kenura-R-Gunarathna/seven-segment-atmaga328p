# Bill of Materials (both boards)

Human-readable component summary for the **signal board** (`../../function_generator.kicad_sch`)
and the **power board** (`../../power_supply_2/`). Grouped by type; counts cross-checked against
the verified KiCad netlists.

> **Authoritative BOM = KiCad → Tools → Generate BOM** (or the netlist). This page is a readable
> overview; regenerate from KiCad before ordering. THT throughout (hand-build).

---

## Signal board — `function_generator`

### Active / ICs
| Qty | Refs | Part | Package | Notes |
|----|------|------|---------|-------|
| 1 | U1 | ATmega32A-P | DIP-40 | MCU, 16 MHz ext crystal |
| 8 | U2, U7, U8, U10, U11, U14, U18, U22 | 74HC595 | DIP-16 | DAC16 ×2 (HW-SPI) + control bus + LED latch |
| 9 | U3,U4,U5,U9,U12,U16,U17,U21,U25 | TL071 | DIP-8 | buffers / gain / offset / centering op-amps (±15 V) |
| 3 | U6, U26, U27 | LM393 | DIP-8 | SAR comparator (U6, +8 V) + 2× overcurrent (U26/U27, +5 V) |
| 1 | U13 | NE5532 | DIP-8 | Sallen-Key output filter |
| 1 | U15 | CD4051B | DIP-16 | output mux (8-ch) |
| 4 | U19, U20, U23, U24 | CD4066BE | DIP-14 | gain-octave analog switches |

### Discretes
| Qty | Refs | Part | Notes |
|----|------|------|-------|
| 1 | Y1 | 16 MHz crystal | + 2× 22 pF (C2/C4) |
| 5 | K1–K5 | SANYOU SRD Form C relay | routing/redirect |
| 2 | Q1, Q3 | BC557 (PNP) | +rail current sense |
| 2 | Q2, Q4 | BC547 (NPN) | −rail current sense |
| 5 | Q5–Q9 | BC337 (NPN) | relay drivers |
| 8 | D1–D8 | LED 3 mm | status (via U2 595, 470 Ω) |
| 11 | D9–D19 | 1N4148 | input clamps (D9–D12), protection (D13/D14), relay flyback (D15–D19) |
| 1 | L1 | 10 µH | AVCC LC filter |
| 1 | SW1 | tactile push (2-pin `SW_Push`) | RESET |

### Resistors (≈150 total — grouped by function)
| Group | Approx qty | Value(s) | Notes |
|-------|-----------|----------|-------|
| R-2R ladders (signal + offset DAC) | ~66 | **20 k / 10 k 1 %** | R24–R56, R57–R89 — **matched, tight, same thermal zone** |
| Binary gain banks (×2) | ~18 | 2.56 M…20 k + 51 k Rf, **1 %** | R100–R108, R109–R117 |
| Status-LED series | 8 | 470 Ω | R5–R12 |
| Protection sense | 4 | 30 Ω | R118/R119/R129/R130 |
| Protection biasing | ~20 | 1 k / 10 k / 11 k / 100 k | window-comparator dividers |
| Relay driver base/pull | 10 | 1 k / 10 k | R140–R149 |
| Centering | ~6 | 10 k / 32 k | |
| Misc (pull-ups, AREF, TX, dividers) | ~12 | 100 Ω, 1 k, 2 k, 4.7 k, 10 k, 20 k 1 W, 100 k, 980 k 1 W | R1–R4, R13–R23 |

### Capacitors (≈54 total)
| Qty | Value | Notes |
|----|-------|-------|
| ~40 | 0.1 µF | per-IC decoupling (one per power pin) |
| 4 | 22 pF | crystal + gain comp |
| 4 | 47 pF | |
| 2 | 1 nF | output filter |
| 1 each | 2.7 nF, 470 pF | filter |
| 1 | 47 µF / 25 V | bulk |

### Connectors
| Qty | Refs | Part | Notes |
|----|------|------|-------|
| 1 | J1 | 2×5 pin header | **AVR-ISP** (MOSI/MISO/SCK/RST/+5/GND) |
| 1 | J2 | 1×4 | UART → HC-06 (RX/TX/+5/GND) |
| 1 | J3 | 1×5 | spare PD4–PD7 |
| 1 | J4 | 1×8 | spare PA1–PA7 |
| 3 | DDS1/DDS2/OSI | BNC | outputs / scope input |
| 3 | 5V_PS1, 8V_PS1, 15V_PS1 | screw terminal (1×02, 1×03, 1×03) | power input from power board |

---

## Power board — `power_supply_2`

### Regulators
| Qty | Refs | Part | Package | Notes |
|----|------|------|---------|-------|
| 5 | U6, U7, U8, U9, U10 | LM2596T-ADJ | TO-220-5 | buck pre-reg (U6/U9 = inverting-buck blocks, **not load-bearing**) |
| 1 | U4 | L7815 | TO-220 | +15 V |
| 1 | U2 | L7808 | TO-220 | +8 V |
| 1 | U1 | L7805 | TO-220 | +5 V |
| 1 | U5 | L7915 | TO-220 | −15 V (from −24) |
| 1 | U3 | L7908 | TO-220 | −8 V (from −24) |
| 5 | RV1–RV5 | Bourns 3296W 10 k trimmer | THT | buck FB set-and-forget |

### Discretes
| Qty | Refs | Part | Notes |
|----|------|------|-------|
| 5 | L1–L5 | 33 µH 3 A inductor | buck |
| ~13 | D1–D5, D7, D10, D12, D13, D15, D17, D20, D22 | 1N4007 (DO-41) | input reverse-protect / output clamp |
| ~7 | D6, D8, D11, D14, D16, D18, D21 | 1N5822 Schottky (DO-201AD) | buck catch diode |
| 2 | D9, D19 | 1N5400 (DO-201AD) | output clamp |
| 10 | HS1–HS10 | heatsink (TO-220/263) | regulators |

### Capacitors
| Qty | Value | Notes |
|----|-------|-------|
| ~10 | 470 µF / 50 V | bulk input |
| 5 | 680 µF / 50 V | buck input |
| 5 | 220 µF / 50 V | buck output |
| ~12 | 0.1 µF | decoupling |
| 3 | 0.33 µF / 50 V | 78xx input |
| 2 | 2.2 µF / 50 V | 79xx input |
| 2 | 1 µF / 50 V | 79xx output |
| ~8 | 10 µF / 50 V | linear output |

### Resistors / Connectors
| Qty | Refs | Value / Part | Notes |
|----|------|--------------|-------|
| 4 | R1, R2, R6, R7 | 47 k | UVLO divider |
| 5 | R3, R4, R5, R8, R9 | 1 k | buck FB bottom |
| 7 | +V1, +V2, −V1, −V2, 5V_PS1, 8V_PS1, 15V_PS1 | screw terminal (1×03 / 1×02) | ±24 input + rail output |
| 5 | J1–J5 | 1×3 header | **redirect / limp-home** (buck-direct vs linear) |

> ⚠️ **Negative buck-direct redirect tap = RAW −24 V** — DMM-verify before jumpering. See
> `discussions/2026-06-13-power-and-pcb.md` and `POWER.md`.
