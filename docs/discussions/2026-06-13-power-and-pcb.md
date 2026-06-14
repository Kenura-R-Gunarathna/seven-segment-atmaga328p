# 2026-06-13 — Power supply (final) + PCB layout

Continuation of `2026-06-12-power-supply.md`. Covers the negative-rail analysis, the move to a
separate power-supply PCB project, and the start of PCB layout.

## 09:30 — Can an LM2596 buck make a negative rail?
**Q:** Feed a buck `VIN=GND, GND-pin=−24` and call OUT `−15` — does it work?
**A / decision:** **No.** It's the recurring trap. A plain (diode) buck can only **source** current
(VIN→OUT); it cannot **sink**. A ground-referenced negative rail must **sink** the current the
op-amps push back into it (their +rail current returns through the −rail). So the −rail node
charges up toward 0 and won't hold. **At no load it reads −15 on a meter — that's the trap — it
collapses under real load.**
**Why:** the failure is *current direction*, not the input voltage. Bench proof: set −15 no-load,
hang ~1.5 kΩ from the −rail **to GND**; a buck climbs toward 0, a 7915/inverting-BB holds −15.

## 09:50 — So how do you make a negative rail with an LM2596?
**A / decision:** Only the **inverting buck-boost** (LM2596 datasheet Fig 24): **inductor shunts
OUT→GND**, the chip's **GND pin floats down to −Vout**, **catch diode flipped** (anode −Vout,
cathode OUT), output cap GND→−Vout. For **−15 from +24** the chip sees `Vin+|Vout| = 39 V` → use
**LM2596HV** + a **60 V Schottky** catch diode; FB divider top→GND, bottom→GND-pin.
**Why:** the inverting topology actively pumps charge *out* of the −Vout node, so it can sink the
load current a plain buck can't. It's NOT a relabeled buck — inductor-to-ground + floating GND pin
is the whole difference.

## 10:10 — Final power architecture
**A / decision:** Two isolated PSUs bonded at GND → split **±24 V** (common ground). Then:
- **+15 / +8 / +5:** LM2596 **buck** (trim ≈ **+18 / +11 / +8**) → linear **7815 / 7808 / 7805**.
- **−15 / −8:** **7915 / 7908 linear straight from −24** (light ~0.1 A loads; 0.9 / 1.6 W).
**Why:** bucks earn their place on the positive/heavy rails (+5 especially); the negatives are
light and a linear from the existing −24 is simplest and can't hit the buck sink problem.

## 10:25 — The inverting-buck blocks (U6/U9) — keep or remove?
**A / decision:** **Keep the footprints, but they are NOT load-bearing** — the 7915/7908 make the
negative rails. As wired they may not pre-regulate (output node sits on the −24 rail they
reference). **Bench-verify**: −V1/−V2 stable + inductor cool → leave on; −24 unchanged + warm
inductor / 150 kHz hash on the rail → **disable** (`~ON/OFF` → GND). Don't leave a rogue switcher
injecting noise onto the analog rails.

## 10:40 — Are buck FB trimmers allowed (no-pots rule)?
**A / decision:** **Yes.** A Bourns **3296W 10 k** trimmer on a buck FB divider is a *fixed
supply-rail* set-and-forget trim — NOT a manual *signal* control. The all-digital rule governs
signal parameters (amplitude/offset/etc.), not power-rail setpoints.
**Why:** set it once with a DMM, lock it (lacquer); never touched during operation.

## 10:55 — Redirect / limp-home jumpers
**A / decision:** Per-rail 3-pin headers select **buck-direct** vs **linear-regulated** output, so
a burned linear can be bypassed without a respin. Procedure: **power off → re-trim buck to the
exact rail → jumper → DMM-verify**. **DANGER: the negative buck-direct tap is RAW −24 V** (not a
trimmed −18/−15) — **must DMM-verify before jumpering** or it destroys the op-amps. (Positive taps
are trimmable bucks, so safer.)

## 11:10 — Power supply as its own KiCad project
**A / decision:** The power supply is a **separate KiCad project** (`power_supply_2`, its own PCB),
not part of the signal board.
**Why:** KiCad is one-PCB-per-project; and physically separating the **buck switching noise** from
the sensitive analog / 16-bit DAC path is good practice. Repair/swap without touching the signal
board. Connected via screw terminals.

## 14:00 — PCB: single-layer hand-drawn feasibility
**A / decision:** **Power board** is reasonable to hand-draw single-sided (few parts + jumpers).
**Signal board is NOT** — ~25 DIP ICs + ~150 resistors + shared buses → needs **2 layers from a
fab** (order it, ~$30; don't pen-plot/etch it).
**Why:** single-layer can't route that density without hundreds of jumpers.

## 14:15 — Hand-draw design rules (0.7 mm copper)
**A / decision:** Board Setup → Constraints: **min track 0.7**, **min clearance 0.7–0.8**, **min
connection width 0.7** (not 0), **min annular 0.5**, **min drill ≥ 0.8** (real pad holes; can't
hand-drill 0.3), copper-to-hole 0.3, hole-to-hole 0.5. Via/uVia settings **irrelevant** (single
layer). Net classes: signal 0.7 mm, **power/GND 1.5–2.5 mm**.

## 14:25 — KiCad won't let me pick 1 copper layer
**A / decision:** KiCad minimum is **2 copper layers** — normal. For single-sided: select 2,
**route everything on B.Cu only**, leave F.Cu empty, use **wire jumpers** to cross, and **plot only
B.Cu** for manufacture. Hide/lock F.Cu so you don't route on it.

## 14:35 — Does the board fit A3 paper?
**A / decision:** **A3 = 297 × 420 mm — fits with margin** (board ~200–250 mm). **Won't fit A4.**
The big pink rectangle on screen is the **drawing-sheet / title-block frame, NOT the board edge** —
the real PCB boundary is what you draw on **Edge.Cuts**. Tip: print 1:1 on A3 and lay real parts on
it to sanity-check footprints before fab.

## 14:50 — Why doesn't the PCB look like my schematic? (the "mess")
**A / decision:** **PCB layout ≠ schematic layout** — KiCad does NOT copy schematic positions.
Footprints land in a pile; the thin lines are the **ratsnest** (one airwire per unrouted
connection) — normal pre-routing. You **floorplan by functional block manually**, nudge parts to
shorten/uncross airwires, then route; airwires vanish as copper is laid. (U3/U4 etc. aren't
"unknown chips" — they're your TL071s; ref designators match the schematic 1:1.)

## 15:10 — Update-PCB footprint errors
**A / decision:** Pad-count mismatches (caught at Update PCB, not ERC):
- **8V_PS1 / 15V_PS1** are `Screw_Terminal_01x03` (3 pins) but had a **1x02** footprint → assign a
  **1x03** terminal block (`...282834-3_1x03...`).
- **SW1** is `SW_Push_Dual` (4 pins) but `SW_PUSH_6mm` pads are numbered **1,1,2,2** (2 nets), so
  pads 3/4 don't exist → **change the SYMBOL to 2-pin `SW_Push`** (reset only needs 2 terminals).
**Why:** swapping the *footprint* alone never fixes it — the *symbol* pin count must match.

## 15:30 — Add 0.1 µF decoupling to the protection / relay sheets?
**A / decision:** **Yes.** Rule: **0.1 µF across every IC's power pins, right at the pins.** The
protection **LM393** comparators (fault detector) and the relay sheets were missing them — add
them (comparators false-trigger / oscillate without). The **relay rail also wants a bulk cap**
(≈10–100 µF) for coil-switching surge; the flyback diode (D15…) is already present.
