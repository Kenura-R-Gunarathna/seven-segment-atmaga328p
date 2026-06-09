"""ATmega32A function generator serial command interface.

Connect USB-UART adapter to ATmega PD0 (RX) and PD1 (TX).
On Linux the port is typically /dev/ttyUSB0 or /dev/ttyACM0.
"""

import time
import serial


class ATmegaSerial:
    def __init__(self, port: str, baud: int = 9600, timeout: float = 1.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        # Flush any boot message
        time.sleep(0.5)
        self.ser.reset_input_buffer()

    def _send(self, cmd: str) -> str:
        """Send command string + newline, return response line."""
        self.ser.write((cmd + '\n').encode())
        time.sleep(0.05)
        lines = []
        deadline = time.time() + 0.3
        while time.time() < deadline:
            if self.ser.in_waiting:
                line = self.ser.readline()
                if line:
                    lines.append(line.decode(errors='replace').strip())
                    break
            time.sleep(0.005)
        return lines[0] if lines else ''

    def set_freq(self, hz: int) -> str:
        resp = self._send(f'f{hz}')
        if not resp.startswith('OK'):
            raise RuntimeError(f'set_freq({hz}) -> {resp!r}')
        return resp

    def set_wave(self, wave: int) -> str:
        resp = self._send(f'w{wave}')
        if not resp.startswith('OK'):
            raise RuntimeError(f'set_wave({wave}) -> {resp!r}')
        return resp

    def status(self) -> str:
        return self._send('s')

    def close(self):
        self.ser.close()


class MockATmega:
    """Offline stand-in — no hardware needed for testing analysis code."""
    def __init__(self):
        self.freq = 1000
        self.wave = 0

    def set_freq(self, hz: int) -> str:
        self.freq = hz
        return f'OK f{hz}'

    def set_wave(self, wave: int) -> str:
        self.wave = wave
        return f'OK w{wave}'

    def status(self) -> str:
        names = ['sin', 'sqr', 'tri', 'saw']
        return f'f{self.freq} w{self.wave} {names[self.wave]}'

    def close(self):
        pass
