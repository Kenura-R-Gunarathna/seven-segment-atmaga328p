#!/usr/bin/env python3
"""
Automated frequency-sweep test for the ATmega32A function generator.

Usage:
    python sweep_test.py [--port /dev/ttyUSB0] [--mock]

    --port   Serial port of the USB-UART adapter connected to ATmega PD0/PD1
             Default: /dev/ttyUSB0
    --mock   Run fully offline (no serial, no scope) using simulated signals

Output:
    results_YYYYMMDD_HHMMSS.png  — bandwidth plot for all 4 waveforms
    results_YYYYMMDD_HHMMSS.csv  — raw measurement table
"""

import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from serial_cmd import ATmegaSerial, MockATmega
from hantek import Hantek6022, MockHantek
from analysis import measure_vpp, measure_freq, measure_thd, find_3db_freq, summarise

# ── sweep config ──────────────────────────────────────────────────────
FREQ_STEPS = [
    10, 20, 50,
    100, 200, 500,
    1000, 2000, 5000,
    10000, 15000, 20000,
]

WAVE_NAMES  = ['sine', 'square', 'triangle', 'sawtooth']
WAVE_COLORS = ['steelblue', 'tomato', 'seagreen', 'darkorange']
SETTLE_S    = 0.25     # seconds to wait after changing frequency
CAPTURE_N   = 16384    # samples per capture (enough for ≥3 cycles at 10 Hz @ 100 kSa/s)
# ─────────────────────────────────────────────────────────────────────


def sweep_wave(atm, scope, wave: int) -> list:
    """Run frequency sweep for one waveform. Returns list of result dicts."""
    print(f'\n=== {WAVE_NAMES[wave].upper()} (w{wave}) ===')
    atm.set_wave(wave)
    if hasattr(scope, 'set_state'):          # MockHantek sync
        scope.set_state(FREQ_STEPS[0], wave)
    time.sleep(0.1)

    results = []
    for hz in FREQ_STEPS:
        atm.set_freq(hz)
        if hasattr(scope, 'set_state'):
            scope.set_state(hz, wave)
        time.sleep(SETTLE_S)

        t, v = scope.capture(CAPTURE_N)
        sr   = scope.sample_rate_hz

        vpp  = measure_vpp(v)
        meas = measure_freq(v, sr)
        thd  = measure_thd(v, sr) if wave == 0 else float('nan')
        err  = abs(meas - hz) / hz * 100.0

        row = {
            'wave':    WAVE_NAMES[wave],
            'set_hz':  hz,
            'meas_hz': round(meas, 1),
            'vpp':     round(vpp, 4),
            'thd_pct': round(thd, 2) if not np.isnan(thd) else '',
            'freq_err_%': round(err, 2),
        }
        results.append(row)

        thd_str = f'  THD={thd:.1f}%' if not np.isnan(thd) else ''
        print(f'  {hz:6d} Hz  Vpp={vpp:.3f}V  meas={meas:8.1f} Hz  err={err:.1f}%{thd_str}')

    return results


def plot_all(all_results: dict, ts: str):
    fig, axes = plt.subplots(2, 2, figsize=(13, 8))
    fig.suptitle('ATmega32A R-2R DAC Function Generator — Frequency Response')

    for wave in range(4):
        res  = all_results[wave]
        freqs = [r['set_hz'] for r in res]
        vpps  = [r['vpp']    for r in res]

        ax = axes[wave // 2][wave % 2]
        ax.semilogx(freqs, vpps, 'o-', color=WAVE_COLORS[wave], linewidth=2,
                    markersize=6, label=WAVE_NAMES[wave])

        # -3 dB line
        bw = find_3db_freq(freqs, vpps)
        ref_vpp = vpps[freqs.index(min(freqs, key=lambda f: abs(f - 100)))]
        thresh  = ref_vpp * 0.7071
        ax.axhline(thresh, color='red', linestyle='--', alpha=0.6, label='-3 dB')
        ax.axvline(bw,     color='red', linestyle=':',  alpha=0.6)

        ax.set_xlabel('Frequency (Hz)')
        ax.set_ylabel('Vpp (V)')
        ax.set_title(f'{WAVE_NAMES[wave].capitalize()}   BW ≈ {bw:.0f} Hz')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.25)
        ax.set_xlim(freqs[0] * 0.8, freqs[-1] * 1.3)

    plt.tight_layout()
    out = Path(f'results_{ts}.png')
    plt.savefig(out, dpi=150)
    print(f'\nPlot saved: {out}')
    plt.show()


def save_csv(all_results: dict, ts: str):
    out = Path(f'results_{ts}.csv')
    fields = ['wave', 'set_hz', 'meas_hz', 'vpp', 'thd_pct', 'freq_err_%']
    with open(out, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for rows in all_results.values():
            w.writerows(rows)
    print(f'CSV  saved: {out}')


def print_summary(all_results: dict):
    print('\n' + '─' * 50)
    print(f'{"Waveform":<12} {"BW -3dB":>10}  {"Vpp@100Hz":>10}  {"THD@1kHz":>10}')
    print('─' * 50)
    for wave, res in all_results.items():
        freqs = [r['set_hz']  for r in res]
        vpps  = [r['vpp']     for r in res]
        thds  = [r['thd_pct'] for r in res]
        s = summarise(freqs, vpps, [t if t != '' else float('nan') for t in thds])
        thd_s = f"{s['thd_pct_1k']:.1f}%" if not np.isnan(s['thd_pct_1k']) else 'N/A'
        print(f'{WAVE_NAMES[wave]:<12} {s["bw_3db_hz"]:>9.0f}Hz'
              f'  {s["ref_vpp"]:>9.3f}V  {thd_s:>10}')
    print('─' * 50)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port',  default='/dev/ttyUSB0')
    ap.add_argument('--baud',  type=int, default=9600)
    ap.add_argument('--mock',  action='store_true',
                    help='Use mock devices (no hardware required)')
    args = ap.parse_args()

    ts = datetime.now().strftime('%Y%m%d_%H%M%S')

    if args.mock:
        print('[MOCK MODE] — no serial port or USB scope needed')
        atm   = MockATmega()
        scope = MockHantek()
        scope.setup()
    else:
        print(f'Connecting to ATmega on {args.port} @ {args.baud} baud...')
        try:
            atm = ATmegaSerial(args.port, args.baud)
        except Exception as e:
            print(f'ERROR: {e}\nTip: check --port, try /dev/ttyACM0')
            sys.exit(1)
        print(f'  Status: {atm.status()}')

        print('Opening Hantek 6022...')
        try:
            scope = Hantek6022()
            scope.setup()
        except Exception as e:
            print(f'ERROR opening scope: {e}')
            print('Tip: check udev rules, pip install hantek6022api')
            atm.close()
            sys.exit(1)

    all_results: dict = {}
    try:
        for wave in range(4):
            all_results[wave] = sweep_wave(atm, scope, wave)
    finally:
        scope.close()
        atm.close()

    print_summary(all_results)
    save_csv(all_results, ts)
    plot_all(all_results, ts)


if __name__ == '__main__':
    main()
