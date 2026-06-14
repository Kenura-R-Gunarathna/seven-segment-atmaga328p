# 2026-06-12 — Power supply

## 10:00 — Why did the linear regulators burn?
**Q:** Burned L7808/L7908 (and a 7805 from 24 V); L7815/L7915 survived. Why?
**A / decision:** Two different failure modes.
- **7805 from +24 V = thermal.** `P = (Vin−Vout)·I`. +5 load is heavy (relays 5×~80 mA + 8 LEDs
  + MCU ≈ 0.15–0.4 A) → 19 V × 0.15–0.4 A = **2.8–6.6 W** → bare TO-220 (~50 °C/W) cooks instantly.
- **±8 regs (cascaded off ±15) = reverse-stress**, not heat. Their load is tiny (mW), but at
  power-down the ±8 output caps held while ±15 collapsed faster → **OUT > IN** → reverse current
  through the reg (no OUT→IN protection diode) → dead.
- **±15 survived:** fed from big +24 input caps, modest op-amp load (~0.9 W).
**Why:** never run a *loaded* linear at a big drop; never cascade linears without an OUT→IN diode.

## 10:20 — Power architecture going forward
**Q:** Use LM2596-ADJ bucks everywhere, then linear post-regs? Headroom?
**A / decision:** **Buck pre-reg + linear post — but linear only where it earns it.** Two isolated
PSUs (S-500 +, **S-250 flipped** → common 0 V) make split **±24**. Buck the heavy/positive rails;
linear post-reg with **≥3 V headroom** (78xx dropout ~2 V — "+2 V" is too low). +5 = pure buck.
**Why:** buck drops the bulk voltage with little heat; a small drop on the linear = little heat.

## 10:35 — Do we even need the linear post-regs?
**Q:** Can a buck give clean enough power for the op-amps?
**A / decision:** **Mostly no, you don't need them.** Op-amp PSRR (~40–60 dB at 150 kHz) +
out-of-band ripple (150 kHz vs ≤20 kHz signal) + a small **LC post-filter (10–22 µH + 100–220 µF)**
→ ~1–5 mV ripple, filtered before the BNC. **Baseline = buck + LC, no linear**; add a linear on a
rail only if bench measurement shows noise you care about (most likely +5, the R-2R DAC reference).
**Why:** the R-2R DAC isn't 16-bit *accurate* (1 % resistors ≈ 6–7 bits) — the "16-bit" is step
resolution, so a few mV of out-of-band ripple is tolerable.

## 11:00 — Buck feedback resistor values (LM2596-ADJ)
**A / decision:** `Vout = 1.23(1 + R_top/R_bot)`, **R_bot = 1.0 k all rails**:
+5 → R_top **3.09 k** (5.03 V) · +8 → **5.49 k** (7.98 V) · +15 → **11.3 k** (15.13 V). E96 1 %.
The "3.1k/11.2k" first drawn aren't standard E-series.

## 11:30 — Negative −15 V rail (the recurring trap)
**Q:** Can I make −15 with an LM2596? (Tried mirroring the +15 buck; then the TI inverting figure.)
**A / decision:** A **plain buck can't output a negative rail** — mirroring the +15 buck fails
whether fed from −24 or +24. Two correct options:
- **Recommended: 7915 linear from −24** (we *have* −24). 9 V × 0.1 A = 0.9 W, heatsink, OUT→IN
  diode. Proven part, no chip stress.
- **Inverting buck-boost (TI Fig 23)** from +24: inductor **shunts OUT→GND**, LM2596 **GND pin
  floats at −15 V**, diode between OUT and −15, FB divider top tied to system GND (11.3k/1k). But
  `Vin+|Vout| = 39 V` → at the standard LM2596's 40 V limit → **use LM2596HV**, and it delivers
  less current (~0.7 A, still fine for 0.1 A).
**Why:** −15 only draws ~0.1 A → the linear is the simplest low-risk path; the inverting BB is
fiddly (mis-wired twice) and stresses the chip.
