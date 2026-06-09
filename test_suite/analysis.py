"""Signal analysis functions: Vpp, frequency, THD, bandwidth estimation."""

import numpy as np
from typing import List


def measure_vpp(v: np.ndarray) -> float:
    """Peak-to-peak voltage."""
    return float(np.max(v) - np.min(v))


def measure_vrms(v: np.ndarray) -> float:
    """RMS voltage (AC component only — DC removed)."""
    return float(np.sqrt(np.mean((v - np.mean(v)) ** 2)))


def measure_freq(v: np.ndarray, sample_rate_hz: float) -> float:
    """FFT fundamental frequency. Returns Hz."""
    n   = len(v)
    ac  = v - np.mean(v)
    win = np.hanning(n)
    mag = np.abs(np.fft.rfft(ac * win))
    frq = np.fft.rfftfreq(n, d=1.0 / sample_rate_hz)
    # Ignore DC bin
    peak = int(np.argmax(mag[1:])) + 1
    # Quadratic interpolation for sub-bin accuracy
    if 1 < peak < len(mag) - 1:
        alpha = mag[peak - 1]
        beta  = mag[peak]
        gamma = mag[peak + 1]
        denom = (alpha - 2 * beta + gamma)
        if denom != 0:
            p = 0.5 * (alpha - gamma) / denom
            return float(frq[peak] + p * (frq[1] - frq[0]))
    return float(frq[peak])


def measure_thd(v: np.ndarray, sample_rate_hz: float,
                n_harmonics: int = 7) -> float:
    """Total Harmonic Distortion in percent (H2-H7 vs fundamental)."""
    n   = len(v)
    ac  = v - np.mean(v)
    win = np.hanning(n)
    mag = np.abs(np.fft.rfft(ac * win))
    frq = np.fft.rfftfreq(n, d=1.0 / sample_rate_hz)
    bin_hz = frq[1]

    fund_idx = int(np.argmax(mag[1:])) + 1
    fund_mag = mag[fund_idx]
    fund_hz  = frq[fund_idx]

    harm_sq = 0.0
    for h in range(2, n_harmonics + 1):
        hf = fund_hz * h
        if hf >= sample_rate_hz / 2:
            break
        hi = int(round(hf / bin_hz))
        # Sum ±1 bin around expected harmonic to handle slight drift
        lo = max(0, hi - 1)
        hi = min(len(mag) - 1, hi + 1)
        harm_sq += float(np.max(mag[lo:hi + 1]) ** 2)

    if fund_mag < 1e-10:
        return float('nan')
    return float(100.0 * np.sqrt(harm_sq) / fund_mag)


def find_3db_freq(freqs_hz: List[float], vpps: List[float],
                  ref_hz: float = 100.0) -> float:
    """
    Locate the -3 dB frequency (Vpp drops to 0.707 × reference).
    ref_hz should be a well-behaved mid-band frequency in your sweep list.
    Returns extrapolated estimate if the rolloff wasn't reached.
    """
    fa = np.array(freqs_hz, dtype=float)
    va = np.array(vpps,     dtype=float)

    ref_idx = int(np.argmin(np.abs(fa - ref_hz)))
    ref_vpp = va[ref_idx]
    thresh  = ref_vpp * 0.7071

    # Walk from ref_idx toward high freqs
    for i in range(ref_idx, len(va) - 1):
        if va[i] >= thresh >= va[i + 1]:
            # Linear interpolation in log-freq space
            logf1, logf2 = np.log10(fa[i]), np.log10(fa[i + 1])
            t = (thresh - va[i]) / (va[i + 1] - va[i])
            return float(10 ** (logf1 + t * (logf2 - logf1)))

    # Not yet reached: fit a 1st-order slope and extrapolate
    if len(fa) >= 2:
        # Use last 3 points for slope
        x = np.log10(fa[-3:])
        y = va[-3:]
        c = np.polyfit(x, y, 1)           # linear in log-freq
        if c[0] < 0:                       # slope is negative
            log_f3db = (thresh - c[1]) / c[0]
            est = 10 ** log_f3db
            return float(est)

    return float(fa[-1])   # fallback: return highest tested freq


def summarise(freqs: List[float], vpps: List[float],
              thds: List[float]) -> dict:
    """Return a summary dict for a single waveform sweep."""
    fa  = np.array(freqs)
    va  = np.array(vpps)
    bw  = find_3db_freq(freqs, vpps)
    ref = vpps[list(freqs).index(min(freqs, key=lambda f: abs(f - 100)))]
    return {
        'bw_3db_hz':   bw,
        'ref_vpp':     ref,
        'max_vpp':     float(np.max(va)),
        'min_vpp':     float(np.min(va)),
        'vpp_droop_%': float(100.0 * (ref - va[-1]) / ref) if ref else 0.0,
        'thd_pct_1k':  float(thds[list(freqs).index(
                            min(freqs, key=lambda f: abs(f - 1000)))])
                        if thds else float('nan'),
    }
