#!/usr/bin/env python3
r"""cap_sim.py - Phase-3 calibration model for the auto-ranging cap meter.

Fits a physical model of the measurement to the bench data from Phases 1-2,
separates the systematic errors, and emits firmware calibration constants plus
per-range coverage predictions.

Model (per measurement), in ticks / ohm / pF:
    dt = p * R_eff * (C + C_stray)
  where  R_eff   = R_nom + R_on         (mux on-resistance, ~constant)
         p       = ln(ratio)/t_tick     (clock + threshold ratio; ideal 6.93e-7)
         C_stray = fixed offset capacitance at the node (pF)

We model the offset as a CONSTANT stray capacitance (range-independent), which
is what the firmware's single CAL_B assumes. Given a trial R_on the model is
linear in (p, p*C_stray), so we 1-D search R_on with an inner least-squares
solve (numpy only).  Firmware then uses:
         C = dt * CAL_K / R_eff + CAL_B,   CAL_K = 1/p,  CAL_B = -C_stray

Run:  python3 tools/cap_sim.py     -> console summary + report/sim_*.png
"""

import os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

LN2  = np.log(2)
TICK = 1e-6                       # 1 us/tick @ 8 MHz, ps=8
P_IDEAL = LN2 * 1e-12 / TICK      # ideal slope = 6.93e-7 ticks/(ohm.pF)
N_MIN, N_MAX = 200, 58000         # good timer window (ticks)

# Bench data: (label, C_nominal_pF, R_nom_ohm, dt_ticks, trust_weight)
# trust: film/known ~1.0; electrolytics (+-20%) downweighted.
DATA = [
    ("10nF",   1.0e4, 99300,   733, 1.0),
    ("50nF",   5.0e4, 99300,  3579, 0.7),
    ("100nF",  1.0e5, 99300,  7788, 1.0),
    ("0.33uF", 3.3e5,   980,   260, 0.6),
    ("1uF",    1.0e6,  9860,  7581, 0.4),
    ("2.2uF",  2.2e6,  9860, 16105, 0.25),
    ("10uF",   1.0e7,  5519, 38161, 0.25),
    ("47uF",   4.7e7,   980, 35963, 0.25),
    ("220uF",  2.2e8,    56, 31617, 0.25),
]
CHAN_R = [10, 56, 554, 980, 5519, 9860, 56370, 99300, 560000, 1050000, 5600000]

C  = np.array([d[1] for d in DATA])
Rn = np.array([d[2] for d in DATA], float)
dt = np.array([d[3] for d in DATA], float)
w  = np.array([d[4] for d in DATA])


def fit(R_on):
    """Linear LSQ for (p, p*C_stray) at given R_on. dt = p*Reff*C + p*Reff*C_stray."""
    Reff = Rn + R_on
    A = np.column_stack([Reff * C, Reff])
    sw = np.sqrt(w)
    coef, *_ = np.linalg.lstsq(A * sw[:, None], dt * sw, rcond=None)
    pred = A @ coef
    sse = np.sum(w * ((pred - dt) / dt) ** 2)   # relative SSE (fair across decades)
    return sse, coef, pred


best = None
for R_on in np.arange(0, 400.0, 1.0):
    sse, coef, pred = fit(R_on)
    if best is None or sse < best[0]:
        best = (sse, R_on, coef, pred)

sse, R_on, coef, pred = best
p, pCstray = coef
C_stray = pCstray / p
CAL_K = 1.0 / p
CAL_B = -C_stray

def human(pf):
    if pf < 1e3:  return f"{pf:6.0f} pF"
    if pf < 1e6:  return f"{pf/1e3:6.1f} nF"
    if pf < 1e9:  return f"{pf/1e6:6.2f} uF"
    return f"{pf/1e9:6.2f} mF"

print("=" * 60)
print("PHASE-3 CALIBRATION FIT  (constant-stray model)")
print("=" * 60)
print(f"  mux on-resistance   R_on    = {R_on:6.0f} ohm")
print(f"  slope               p       = {p:.4e}  (ideal {P_IDEAL:.4e},"
      f" corr {p/P_IDEAL:.3f})")
print(f"  stray / offset      C_stray = {C_stray:7.1f} pF")
print(f"     -> the offset is range-independent, so a single CAL_B holds.")
print()
print("  Firmware (use EFFECTIVE R = R_nom + R_on):")
print(f"     #define CAL_K  {CAL_K:.0f}UL")
print(f"     #define CAL_B  {CAL_B:.0f}L")
print("     static const uint32_t MUX_R[] = {")
print("        " + ", ".join(f"{int(round(r + R_on))}UL" for r in CHAN_R))
print("     };  // R_nom + R_on")
print()
print("  Measured vs fitted:")
print(f"    {'cap':>7} {'R_nom':>7} {'dt':>7} {'fit':>7} {'err%':>6}")
for (lab, Cn, Rnn, dtn, ww), pr in zip(DATA, pred):
    print(f"    {lab:>7} {Rnn:>7} {dtn:>7.0f} {pr:>7.0f} {100*(pr-dtn)/dtn:>6.1f}")
print()
print("  Per-channel coverage (effective R):")
print(f"    {'ch':>3} {'R_eff':>9} {'C_min':>10} {'C_max':>10}")
for ch, r in enumerate(CHAN_R):
    Reff = r + R_on
    print(f"    R{ch:<2} {int(Reff):>9} {human(N_MIN/(p*Reff)):>10} "
          f"{human(N_MAX/(p*Reff)):>10}")

# ---- plots ----
os.makedirs("report", exist_ok=True)
plt.figure(figsize=(6, 4))
lim = max(dt.max(), pred.max()) * 1.1
plt.plot([0, lim], [0, lim], 'k--', alpha=0.5, label="ideal y=x")
sc = plt.scatter(dt, pred, c=w, cmap="viridis", s=60, zorder=3)
for (lab, *_), x, y in zip(DATA, dt, pred):
    plt.annotate(lab, (x, y), fontsize=7, xytext=(4, 4), textcoords="offset points")
plt.colorbar(sc, label="trust weight")
plt.xlabel("measured Δt (ticks)"); plt.ylabel("model Δt (ticks)")
plt.title(f"Model fit (R_on≈{R_on:.0f} Ω, slope corr {p/P_IDEAL:.3f})")
plt.legend(); plt.grid(alpha=0.3); plt.tight_layout()
plt.savefig("report/sim_fit.png", dpi=140)

plt.figure(figsize=(7, 4))
for ch, r in enumerate(CHAN_R):
    Reff = r + R_on
    cmin = N_MIN / (p * Reff); cmax = N_MAX / (p * Reff)
    plt.plot([cmin, cmax], [ch, ch], lw=6, solid_capstyle="butt", alpha=0.7)
    plt.text(cmax * 1.15, ch, f"R{ch}", va="center", fontsize=8)
plt.xscale("log")
plt.xlabel("Capacitance (pF)"); plt.ylabel("channel")
plt.title("Per-channel coverage windows (overlapping ladder)")
plt.grid(alpha=0.3, which="both"); plt.tight_layout()
plt.savefig("report/sim_coverage.png", dpi=140)
print("\nwrote report/sim_fit.png, report/sim_coverage.png")
