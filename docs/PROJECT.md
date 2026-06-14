# Project — Digital Function Generator + DIY Oscilloscope (ATmega32A)

## Description
A bench **function generator combined with a DIY oscilloscope**, built around a bare-metal
**ATmega32A @ 16 MHz** (external crystal, no Arduino/HAL — direct register access via avr-libc).
Every adjustable parameter is set **digitally by the MCU** — there are no manual pots or switches
in the signal path. The generator synthesizes sine / square / triangle / saw via a 32-bit
phase-accumulator **DDS**, output through a **16-bit R-2R DAC** for the waveform and a second
**16-bit R-2R DAC** for offset/centering. The scope side digitizes an input via a 16-bit **SAR**
(DAC + LM393 comparator) and the internal 10-bit ADC. Control + telemetry is over **UART → HC-06
Bluetooth**. Bench-only, ≤ 30 V, no mains-referenced isolation.

## Objectives
- **Fully digital control — no manual/external controllers.** Waveform, frequency, **amplitude**,
  **DC offset**, centering, mode, and output-filter select are all set by the ATmega32A via the
  R-2R DACs, MCU-driven 74HC595 → CD4066/CD4051 banks, and firmware. No analog pots, no manual
  switches, no digital-pot/PGA controller ICs. The only "switches" are CD4066/CD4051 driven by the
  MCU through shift registers.
- **Two deliberate 16-bit R-2R DACs** (signal + offset). 16-bit is intentional: 76 µV steps let the
  output land on exact round values (10 mV, 100 mV) that 8-bit (19.5 mV) cannot.
- Combined FG + scope modes (FG-only, scope-only, both) selectable in firmware.

Full hard-rules and conventions: see `../CLAUDE.md`.

## Architecture — two boards
1. **Signal board** (`../function_generator.kicad_sch`) — MCU, dual 16-bit R-2R DACs, op-amp
   gain/offset/centering stages, output mux + filter, SAR scope front-end, dual-rail overcurrent
   protection, relays, status LEDs, ISP/UART headers, BNCs. Schematic **done & ERC-clean**.
2. **Power board** (`../power_supply_2/`, its own KiCad project + PCB) — two isolated PSUs bonded at
   GND → split **±24 V**; **+15/+8/+5** via LM2596 buck → 7815/7808/7805; **−15/−8** via 7915/7908
   linear from −24. Kept separate to isolate buck switching noise from the analog path. See
   `hardware/POWER.md`.

## Status
- Hardware **schematic done & ERC-clean** (both boards).
- **PCB layout in progress** (signal board → 2-layer fab; power board → can be single-sided).
- **Firmware** written incrementally by the project owner (verified on hardware).

## Tools used
| Tool | Use |
|------|-----|
| **KiCad 10** | Schematic + PCB for both boards (signal + `power_supply_2`) |
| **avr-gcc toolchain** (`avr-gcc`, `avr-objcopy`, `avr-objdump`, `avr-size`) | Build firmware |
| **avrdude + USBasp** | Flash the ATmega32A (ISP header J5; `make program`) |
| **schemdraw** | Explanatory schematic/block figures → `docs/diagrams/` |
| **Hantek 6022** scope + Python `test_suite/` | Hardware-in-the-loop Vpp/THD/−3 dB sweeps |
| **HC-06** Bluetooth-UART | Wireless command/telemetry link (9600 8N1) |
| **DMM** | Rail trimming + bench verification (esp. buck setpoints, redirect jumpers) |
| **2× enclosed bench PSUs** (S-500 / S-250) | Isolated supplies → split ±24 V input |

## Where to start
- **Firmware:** `hardware/HARDWARE.md` (pins/buses/IRQ) + `../CLAUDE.md` (driver rules).
- **Circuit:** `hardware/FINAL_CIRCUIT.md`. **Power:** `hardware/POWER.md`. **BOM:** `hardware/BOM.md`.
- **Why a decision was made:** `discussions/`.
