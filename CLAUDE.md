# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Bare-metal AVR firmware for an **ATmega32A @ 16 MHz** (external crystal) implementing a combined **function generator + DIY oscilloscope** for a campus project. No Arduino/HAL — direct register access via avr-libc. The matching hardware lives in a separate KiCad project (`../function_generator.kicad_sch`); `OSCILLOSCOPE_FRONTEND.md` is the hardware front-end reference and `CIRCUIT_CHANGES.md` is the pending KiCad rework list (amp gains, DAC wiring, open questions).

> **HARDWARE HARD RULE:** the design uses **two 16-bit R-2R DACs** (2× 74HC595 each) — one for the signal waveform, one for the offset/centering voltage. **Never propose removing or downgrading either 16-bit DAC.** 16-bit is deliberate: 76 µV steps let the output land on exact round values (10 mV, 100 mV); 8-bit (19.5 mV) cannot.

> **PRIMARY DESIGN OBJECTIVE — fully digital control, NO manual/external controllers:** Every adjustable parameter — waveform, frequency, **amplitude**, **DC offset**, centering, mode, output filter select — is set **digitally by the ATmega32A** via the R-2R DACs, MCU-driven 74HC595 → CD4066 banks, and firmware. **Absolutely no manual potentiometers, no manual switches, and no external controller ICs (digital pots, PGAs, etc.).** Consequences for proposals:
> - **Fine amplitude/offset** = digital wavetable scaling of the 16-bit DACs (keep ≥ 50 % scale to preserve resolution), combined with **MCU-controlled CD4066 coarse gain octaves**. Never an analog pot or digital-pot chip.
> - **Centering** = a single fixed resistor injecting −8 V into the U7/U9 summing node; drift trimmed by the offset DAC in firmware. No switched centering bank.
> - The only allowed "switches" are CD4066/CD4051 driven by the MCU through shift registers — i.e. digitally commanded, never hand-operated.

## IC documentation convention (ALWAYS follow when discussing any IC)

When proposing, reviewing, or documenting **any IC** in this design, always state these four things:

1. **Operating / supply voltage** — e.g. ±15V (op-amps), +5V (logic/595/6N137), +8V/−8V (CD4051/CD4066). Always flag **logic-level compatibility**: a CMOS analog part (CD4051/CD4066) at VDD=+8V needs VIH ≈ 0.7×VDD ≈ 5.6V, so 5V logic from a 74HC595 is marginal — prefer VDD=+5V for those parts when the signal range allows.
2. **Pin-definition table** — one row per relevant pin, in this format:

   | Pin | Name | Type | Connects to |
   |-----|------|------|-------------|
   | 14 | SER | Input | PB5_DAC16_R2R_SER |

3. **Electrical type per pin** — Input / Output / Bidirectional / Power / Passive (drives ERC and KiCad label/sheet-pin direction).
4. **Hierarchical-sheet recommendation** — whether the IC (or its functional block) should be its own KiCad sub-sheet, and its interface: **signal pins as hierarchical pins (IN/OUT…), power via global symbols** (`+15V`/`−15V`/`+5V`/`GND` — no power pins). Sub-sheet single-use blocks for organization (e.g. DAC16_R2R, OutputFilter, OutputMux); reuse the same `.kicad_sch` file for duplicated blocks (signal vs offset DAC).

Net-naming convention for this project: **`PB<pin>_<BLOCK>_<SIGNAL>`** (e.g. `PB5_DAC16_R2R_SER`) — pin-prefixed, descriptive, because net names carry into the PCB and aid trace identification. Shared buses use the bus/block name; per-device latches name the specific device (`SIG`/`OFF`/`MUX`/`GAIN`).

## Build & flash

```sh
make                 # build -> build/<dir>.hex (+ .elf .lss .sym .map diagnostics)
make program         # flash via USBasp (normal speed, after fuses are set)
make fuse            # write LFUSE=0xFF HFUSE=0xC9 (16 MHz ext crystal, JTAG off)
make program-slow    # fuse + flash on a FRESH chip — requires JP3 closed on USBasp
make size            # avr-size memory report
make clean
```

There is **no host-side unit test for the firmware** — it is verified on hardware. Toolchain required: `avr-gcc`, `avr-objcopy`, `avr-objdump`, `avr-size`, `avrdude`.

**Fresh-chip gotcha:** a blank ATmega32A runs on the 1 MHz internal RC; USBasp ISP must be ≤250 kHz. Close the **JP3** jumper on the USBasp (forces 8 kHz) and use `make program-slow` for the first flash. Remove JP3 once the 16 MHz crystal is active.

## Hardware-in-the-loop tests (`test_suite/`)

```sh
cd test_suite
pip install -r requirements.txt
python sweep_test.py --mock              # offline: simulated signals, no hardware
python sweep_test.py --port /dev/ttyUSB0 # real: USB-UART on PD0/PD1 + Hantek 6022 scope
```

`sweep_test.py` drives the firmware over UART (`serial_cmd.py`), captures with a Hantek 6022 (`hantek.py`), analyses Vpp/THD/−3 dB (`analysis.py`), and writes `results_*.csv` + `.png`. Both the serial interface and scope have `Mock*` classes so `--mock` runs end-to-end with no devices attached.

## Architecture

### Single translation unit + header-only drivers — read this first

Drivers in `src/drivers/*.h` **define** their functions in the header (not just declare), and are `#include`d **only into `src/main.c`**. The whole firmware compiles as essentially one translation unit. The Makefile globs every `src/**/*.c`, but **`millis.c` is the only standalone `.c`** — it is compiled separately and exposes `millis()` via an `extern` declaration in the headers that need it.

Consequence: **never `#include` a driver header from a second `.c` file** — it will cause duplicate-symbol link errors. Add new functionality as a header-only driver included from `main.c`, or fold it into `main.c`.

### main.c must stay register-free

`main.c` calls only driver functions (`dds_init()`, `led_set_state()`, `cmd_poll()`, …) — **no raw `DDRx`/`PORTx`/`TCCRx`/`TIMSK` writes**. All peripheral register manipulation belongs inside a driver. When porting to another MCU, keep every existing driver.

### Timer allocation (do not double-assign)

- **Timer0** → `millis.c`, CTC /64, OCR0=249 → 1 kHz tick (`TIMER0_COMP_vect`)
- **Timer2** → `dds.h`, CTC /8 → 40 kHz DDS sample ISR (20 kHz in BOTH mode)
- **Timer1** → free

### Signal generation path

`dds.h` runs a 32-bit phase-accumulator DDS in the Timer2 ISR: `phase_inc = freq·2³²/sample_rate`, top 8 bits index a 256-entry PROGMEM sine table (`wavetable.h`); square/triangle/saw are computed inline. Output goes to an 8-bit R-2R DAC on PORTC (note: `dds.h` bit-reverses each sample because the DAC MSB is wired to PC0). A newer **16-bit R-2R DAC** (`dac16.h`) drives two daisy-chained 74HC595s over hardware SPI (SER←PB5/MOSI, SRCLK←PB7/SCK, RCLK←PB3 latch).

**SPI bit-order conflict:** `spi595.h` (legacy display chain) inits SPI as **DORD=1 (LSB-first)**; `dac16.h` needs **DORD=0 (MSB-first)** and re-inits `SPCR` itself. Also: ATmega32A drops to SPI slave mode if **PB4 (/SS)** floats low — `dac16_init()` forces PB4 as output. Both are easy to regress.

### Operating modes (`mode.h`)

`mode_set()` reconfigures Timer2 live: `MODE_FG` (40 kHz, FG only), `MODE_OSC` (Timer2 off, SAR scope runs in main loop), `MODE_BOTH` (Timer2 halved to 20 kHz so the CPU has windows for SAR steps; FG max drops to 10 kHz). Because the sample rate changes, `dds_set_freq_raw(hz, rate)` recomputes `phase_inc` for the new rate (plain `dds_set_freq()` assumes 40 kHz).

### Oscilloscope (`sar_osc.h`, `adc_int.h`)

Two ADC paths share one biased input node: a 16-bit **SAR** built from `dac16.h` + an external comparator on **PD2** (binary search, ~6 kSPS, precise), and the **internal 10-bit ADC** on PA0 (`adc_int.h`, ~38 kSPS, fast). A 2.5 V input bias maps ±2.5 V probe range to 0–5 V so negative voltages can be measured (decode: `mv = (raw − mid)·Vfs/mid`).

### UART command protocol (`cmd.h`)

Line-based (CR/LF terminated) over UART → HC-06 Bluetooth, **9600 8N1, U2X double-speed**. `cmd_poll()` is called every main-loop iteration. Commands: `f<hz>`, `w<0-3>`, `m<0-2>`, `o`/`os<n>` (SAR), `oi`/`oi<n>` (internal ADC), `d<hex4>` (raw DAC test), `s` (status). Replies `OK …` / `ERR …`. ATmega32A quirk handled in `uart.h`: UCSRC and UBRRH share an address — set URSEL (bit 7) when writing UCSRC.

### Legacy drivers — keep them

`src/drivers/` also contains a full capacitance-meter / 7-seg-display stack from a prior project on the same board: `capmeas.h`, `display.h`, `seg.h`, `digit.h`, `scroll.h`, `charset.h`, `mux4067.h`, `comparator.h`, `adc.h`, `hc06.h`. They are intentionally retained for reuse and must not be deleted when reworking `main.c`.

## Conventions

- `main.c` is frequently swapped between a minimal bring-up test (blink / DAC sweep / "Hello World") and the full Phase 2.0 app during hardware debugging. Check its current contents before assuming it's the full firmware.
- Source files use UTF-8 (em-dashes, en-dashes) in comments — match exact bytes when editing with string replacement, or use Write.
- Build flags optimise for size (`-Os`, `--gc-sections`); flash budget is 32 KB, RAM 2 KB.
