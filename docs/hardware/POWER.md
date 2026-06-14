# POWER.md — supply tree, burn post-mortem, redesign

Power for the FG/scope board. Built **separate** from the main board. This doc captures *why the
linear cascade burned* and the decided **buck-pre + linear-post** architecture.

Companion: `HARDWARE.md` (IC supply table), `FINAL_CIRCUIT.md`. Bench notes go in
`../experiments/`; design Q&A in `../discussions/`.

> **Safety:** the enclosed PSUs are **mains-input** — live AC at their terminals (inside the
> metal box). The board only ever sees ≤ 24 V. Common the supply grounds at **exactly one point.**

---

## 1. Why the old supply burned (post-mortem)

Linear-regulator dissipation: **`P = (Vin − Vout) × I_load`** — and it all becomes heat in the
TO-220. A bare TO-220 is ~50 °C/W, so even ~2.8 W ≈ +140 °C rise → junction blows past 125 °C.

| Symptom | Cause |
|---------|-------|
| **7805 from +24 V died** | +5 V load is heavy (relays 5×~80 mA, 8 LEDs, MCU, logic ≈ 150–400 mA). 19 V drop × 0.15–0.4 A = **2.8–6.6 W** → thermal death. A 7805 can never survive +5 from 24 V at this load. |
| **L7808 / L7908 (±8) died** | They were **cascaded** off ±15 with **no output→input diode**. At power-down the ±8 output caps held while ±15 collapsed faster → **OUT > IN → reverse current** through the reg → dead. (Their load is tiny, so it was *not* heat — it was reverse-stress.) |
| **L7815 / L7915 (±15) survived** | Fed from the big +24 input caps (held up at power-down), modest op-amp load (9 V × ~0.1 A ≈ 0.9 W). |

**Lessons:** (1) never run a heavily-loaded linear at a big drop — pre-drop with a buck.
(2) never cascade linears without an OUT→IN protection diode. (3) keep linear headroom small.

---

## 2. Decided architecture — buck pre-reg, linear only where it earns it

**Input:** two isolated enclosed PSUs. The **S-250 is flipped** to make the negative rail:
```
 S-500 (+24, isolated):  V+ = +24V          V− ─┐
 S-250 (24V,  isolated):  V+ ─┘              V− = −24V
                          └──── common ──────┘  = system 0V (GND)   ← single tie point
```
Result: a split **±24 V** with one common ground (`+24V2` / `−24V1` on the schematic).

**Rails (the rule: buck the heavy/positive rails; linear only the op-amp rail, with ~3 V headroom):**

| Rail | Path | Notes |
|------|------|-------|
| **+5 V** | +24 → **LM2596-ADJ buck** (U33) | digital load → buck alone is fine; no linear |
| **+15 V** | +24 → **LM2596-ADJ buck** (U34) → +15 | op-amps. Buck is clean enough with its LC; add a 7815 only if bench shows ripple |
| **−15 V** | **−24 → 7915 linear** *(recommended)* | ~0.1 A × 9 V = 0.9 W → small heatsink. Simplest, proven (the 7915 survived). |
| **+8 V** | +24 → buck **or** +24 → 7808 direct | tiny load (U31 comparator). If linear, feed from +24 *directly* (uncascaded) + OUT→IN diode |
| **−8 V** | −24 → 7908 direct (uncascaded) + OUT→IN diode | tiny load (CD4051/CD4066 VSS + R83/R86); ~16 V × few mA = mW, harmless |

**Do we even need the linears?** Mostly **no.** A buck + small **LC post-filter (10–22 µH +
100–220 µF low-ESR)** gives ~1–5 mV ripple at 150 kHz, which is *out of the ≤20 kHz output band*
and rejected ~40–60 dB by op-amp PSRR. **Baseline = buck + LC on every rail, no linear.** Add a
linear post-reg on a rail **only if** bench measurement shows switching noise you care about
(most likely +5, since it's the R-2R DAC reference). `−15` stays linear simply because it's the
easy, proven, low-risk choice for the op-amp rail.

---

## 3. Buck design — verified values

**LM2596-ADJ:** `Vout = 1.23 V × (1 + R_top/R_bot)`, R_top = OUT→FB, R_bot = FB→GND, FB sensed
**after the inductor**. Catch diode 1N5822 (cathode → OUT/switch node). L = 68 µH 3A,
Cout = 220 µF/50 V low-ESR.

| Rail | R_top (OUT→FB) | R_bot (FB→GND) | Vout |
|------|----------------|----------------|------|
| +5 V | **3.09k 1%** (E96; 3.1k drawn ≈ 5.04 V) | 1k | 5.04 V |
| +15 V | **11.3k 1%** (E96; 11.2k drawn ≈ 14.99 V) | 1k | 15.1 V |

> The drawn 3.1k / 11.2k aren't standard E-series — use **3.09k** and **11.3k (E96)**, or
> 3.0k → 4.92 V and 11k → 14.8 V (both fine).

---

## 4. Negative rails — the gotcha

A standard LM2596 is a **positive buck**: a plain-buck layout (inductor in series OUT→output,
GND pin at system ground) **cannot output a negative rail**, no matter what you feed it. Mirroring
the +15 buck does not work.

**Recommended:** `−24 → 7915 → −15` (linear, 0.9 W, heatsink). `−24 → 7908 → −8` (tiny load).
Add an **OUT→IN protection diode** (1N4148/1N4007, anode = OUT, cathode = IN) across each.
Lowest-risk, proven, no chip stress.

**If you want an LM2596 negative rail — use the inverting buck-boost (TI datasheet Fig 23),**
which makes −15 from the **+24** rail (you don't even need −24):
```
 +24V ──► VIN(1) ;  ON/OFF(5) → tied to GND-pin ;  GND-pin(3) → −15V rail
 OUT(2) ──┬── L 33µH ── system GND        ← inductor SHUNTS to ground (not in series)
          └── D 1N5822: cathode → OUT,  anode → −15V
 −15V ──── Cout 220µF → GND ;  FB(4) ← R_top/R_bot divider from −15V to GND-pin
```
- The LM2596's **GND pin floats at −15 V**; the inductor goes to ground; the diode flips.
- FB regulates the **full |Vout| magnitude** → divider is for 15 V (**11.3k / 1k**), same as the
  +15 buck.
- ⚠️ **Stress:** the chip sees `Vin + |Vout| = 24 + 15 = 39 V` — right at the standard LM2596's
  40 V limit. **Use the LM2596HV (60 V)** for margin.
- ⚠️ Inverting buck-boost delivers **less current** than a buck (~0.7 A) — fine for the ~0.1 A
  op-amp load.

---

## 5. Per-reg hygiene (don't skip)
- **OUT→IN protection diode** on every linear (anode OUT, cathode IN) — the missing piece that
  let the ±8 cascade die.
- **Reverse-polarity diode** at each PSU input (1N400x series) — already on the board.
- Input + output caps **close** to each linear (78xx: 0.33 µF in, ≥0.1 µF + 10 µF out) and each
  buck (low-ESR).
- **Heatsinks** on any TO-220 dissipating > ~0.5 W (the +15/−15 linears, +5 buck runs cool).
- **Single common-ground tie point** between the two PSUs — do not bond their outputs at
  multiple places (ground loops / mis-references).
- PSUs (S-500 / S-250) are massively oversized for a <1 A board — harmless, just big.
