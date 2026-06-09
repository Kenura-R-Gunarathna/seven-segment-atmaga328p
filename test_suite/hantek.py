"""Hantek 6022BE/BL scope wrapper.

Install driver:
  pip install hantek6022api
  sudo apt install libusb-1.0-0-dev
  # udev rule so non-root can access USB:
  echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="04b5", ATTR{idProduct}=="6022", MODE="0666"' | sudo tee /etc/udev/rules.d/99-hantek.rules
  sudo udevadm control --reload-rules && sudo udevadm trigger

Sample rate codes (kSa/s):
  0x09 = 100   0x0a = 50   0x0b = 20   0x0c = 10

Voltage range codes (sensitivity):
  0x01 = 5V    0x02 = 2.5V   0x05 = 1V   0x0a = 0.5V
"""

import numpy as np
from typing import Tuple

# ── real scope ──────────────────────────────────────────────────────────
_SRATE_MAP = {
    0x09: 100_000,   # 100 kSa/s
    0x0a:  50_000,
    0x0b:  20_000,
    0x0c:  10_000,
}

_VDIV_MAP = {
    0x01: 5.0,
    0x02: 2.5,
    0x05: 1.0,
    0x0a: 0.5,
    0x0b: 0.25,
}


class Hantek6022:
    def __init__(self):
        from PyHT6022.LibUsbScope import Oscilloscope
        self._scope   = Oscilloscope()
        self._srate   = 0x09            # 100 kSa/s default
        self._vrange  = 0x0a            # 0.5V/div default
        self._srate_hz = _SRATE_MAP[self._srate]

    def setup(self):
        self._scope.setup()
        self._scope.setNumChannels(1)
        self._scope.setSampleRate(self._srate)
        self._scope.setChannel1VRange(self._vrange)

    def set_srate(self, code: int):
        self._srate    = code
        self._srate_hz = _SRATE_MAP[code]
        self._scope.setSampleRate(code)

    def set_vrange(self, code: int):
        self._vrange = code
        self._scope.setChannel1VRange(code)

    def auto_vrange(self, vpp_est: float):
        """Pick smallest range that fits vpp_est without clipping."""
        for code in sorted(_VDIV_MAP, key=lambda c: _VDIV_MAP[c]):
            if vpp_est < _VDIV_MAP[code] * 10.0 * 0.85:
                self.set_vrange(code)
                return

    def capture(self, n: int = 8192) -> Tuple[np.ndarray, np.ndarray]:
        """Return (time_s, voltage_v) for channel 1, n samples."""
        raw, _ = self._scope.getSamples(n)
        # Ch1 = every byte; 8-bit unsigned centred at 128
        ch1 = np.array(raw, dtype=np.float64)
        vdiv = _VDIV_MAP[self._vrange]
        # Full scale = 10 × V/div, centred at 0V
        voltage = (ch1 - 128.0) / 128.0 * (5.0 * vdiv)
        t = np.arange(len(ch1)) / self._srate_hz
        return t, voltage

    @property
    def sample_rate_hz(self) -> int:
        return self._srate_hz

    def close(self):
        self._scope.closeDevice()


# ── mock scope (for offline analysis testing) ───────────────────────────
class MockHantek:
    """Generates synthetic waveforms — no USB scope needed."""
    def __init__(self):
        self._srate_hz = 100_000
        self._freq     = 1000
        self._wave     = 0
        self._vpp      = 4.0   # realistic R-2R output

    def setup(self): pass
    def set_srate(self, code: int): self._srate_hz = _SRATE_MAP.get(code, 100_000)
    def set_vrange(self, code: int): pass
    def auto_vrange(self, vpp_est: float): pass
    def close(self): pass

    @property
    def sample_rate_hz(self) -> int:
        return self._srate_hz

    def set_state(self, freq: int, wave: int):
        self._freq = freq
        self._wave = wave

    def capture(self, n: int = 8192) -> Tuple[np.ndarray, np.ndarray]:
        t  = np.arange(n) / self._srate_hz
        ph = 2 * np.pi * self._freq * t
        a  = self._vpp / 2.0

        if self._wave == 0:          # sine
            v = a * np.sin(ph)
        elif self._wave == 1:        # square
            v = a * np.sign(np.sin(ph))
        elif self._wave == 2:        # triangle
            v = (2 * a / np.pi) * np.arcsin(np.sin(ph))
        else:                        # sawtooth
            v = a * (2 * ((self._freq * t) % 1.0) - 1.0)

        # Add realistic R-2R bandwidth roll-off above ~15kHz
        from scipy.signal import butter, filtfilt
        if self._freq > 5000:
            b, a_coef = butter(1, 15000 / (self._srate_hz / 2), btype='low')
            v = filtfilt(b, a_coef, v)

        v += np.random.randn(n) * 0.015
        return t, v
