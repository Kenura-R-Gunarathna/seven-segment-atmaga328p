# Function Generator + Oscilloscope — ATmega32A firmware

Bare-metal AVR firmware for an **ATmega32A @ 16 MHz** implementing a combined **function
generator + DIY oscilloscope** (campus project). No Arduino/HAL — direct register access via
avr-libc. The matching hardware is a hand-built KiCad project in the **parent directory**
(`../function_generator.kicad_sch`); the schematic is ERC-clean and verified.

> **Status:** hardware schematic done & verified. Firmware is written incrementally by the
> project owner. The power supply is a **separate board** (see `docs/hardware/POWER.md`).

## Repo layout
```
source_code/
├── src/                  firmware (header-only drivers + main.c) — see CLAUDE.md
│   ├── main.c, utils.h
│   └── drivers/*.h, millis.c
├── test_suite/           hardware-in-the-loop tests (Python; Hantek 6022 + UART)
├── tools/                helper scripts
│   └── schematic/        schemdraw scripts → render to docs/diagrams/
└── docs/
    ├── hardware/         AS-BUILT reference (code against these)
    │   ├── HARDWARE.md        ← pin/bus/relay/IRQ/ISP map (start here for firmware)
    │   ├── FINAL_CIRCUIT.md   ← master block diagram
    │   ├── POWER.md           ← supply tree + burn post-mortem + redesign
    │   ├── PROTECTION_OUTPUT.md, OSCILLOSCOPE_FRONTEND.md, SCOPE_DIGITIZER.md
    ├── design/           REQUIREMENTS.md, output_stage_math.qmd
    ├── diagrams/         schematic SVGs / interactive HTML
    ├── discussions/      design Q&A + decision log (maintained by /qna)
    └── experiments/      bench tests / WIP ideas (TEMPLATE.md)
```
Full doc index: **`docs/README.md`**. Project rules + conventions: **`CLAUDE.md`**.

## Build & flash
```sh
make            # -> build/<dir>.hex (+ .elf .lss .sym .map)
make program    # flash via USBasp (after fuses set)
make fuse       # LFUSE=0xFF HFUSE=0xC9 (16 MHz ext crystal, JTAG off)
make program-slow   # fresh chip: JP3 closed on USBasp (8 kHz) for first flash
make size       # avr-size report
make clean
```
Toolchain: `avr-gcc`, `avr-objcopy`, `avr-objdump`, `avr-size`, `avrdude`. ISP via header **J5**
(see `docs/hardware/HARDWARE.md §7`). No host-side firmware unit test — verified on hardware.

## Where to start
- **Project overview / objectives / tools:** `docs/PROJECT.md`.
- **Coding firmware:** `docs/hardware/HARDWARE.md` (pins/buses/IRQ) + `CLAUDE.md` (driver rules).
- **Understanding the circuit:** `docs/hardware/FINAL_CIRCUIT.md`.
- **Components / BOM:** `docs/hardware/BOM.md`.
- **Power board:** `docs/hardware/POWER.md`.
- **Logging a design decision:** run `/qna` → appended to `docs/discussions/`.
