#!/usr/bin/env python3
"""bt_clock.py — TUI to manage the HC-06 clock over Bluetooth (RFCOMM/SPP).

Live stream of incoming "HH:MM" lines on top, an input box at the bottom to
send commands, and a status bar showing the last received time + link state.
No rfcomm binary needed — raw Bluetooth socket.

Setup (Arch/CachyOS):   sudo pacman -S python-prompt_toolkit
        (other distros): pip install prompt_toolkit

Usage:
    ./bt_clock.py                       # default MAC below
    ./bt_clock.py 98:DA:60:0F:A8:4E     # or pass the MAC
    ./bt_clock.py --log clock.log       # tee the stream to a file

In the input box:
    21:45  (or 2145)  -> set the clock; AVR replies "OK 21:45"
    q / Ctrl-C        -> quit
"""

import argparse
import re
import socket
import sys
import threading

try:
    from prompt_toolkit import Application
    from prompt_toolkit.buffer import Buffer
    from prompt_toolkit.layout import Layout, HSplit, Window
    from prompt_toolkit.layout.controls import BufferControl, FormattedTextControl
    from prompt_toolkit.key_binding import KeyBindings
    from prompt_toolkit.styles import Style
except ImportError:
    sys.exit("missing prompt_toolkit -> sudo pacman -S python-prompt_toolkit "
             "(or: pip install prompt_toolkit)")

DEFAULT_MAC = "98:DA:60:0F:A8:4E"
CHANNEL = 1
TIME_RE = re.compile(rb"(\d{2}:\d{2}(?::\d{2})?)")


class BTClock:
    def __init__(self, mac, channel, logfile):
        self.mac = mac
        self.channel = channel
        self.logfile = logfile
        self.sock = None
        self.connected = False
        self.last_time = "--:--"
        self.lines = []          # rolling log of received lines

        # ── widgets ──
        self.log_buf = Buffer(read_only=False)
        self.input_buf = Buffer(multiline=False, accept_handler=self._on_enter)

        kb = KeyBindings()

        @kb.add("c-c")
        @kb.add("c-q")
        def _(event):
            event.app.exit()

        body = Window(BufferControl(self.log_buf), wrap_lines=True)
        status = Window(FormattedTextControl(self._status), height=1,
                        style="class:status")
        prompt = Window(BufferControl(self.input_buf), height=1,
                        get_line_prefix=lambda *_: [("class:prompt", "> ")])

        self.app = Application(
            layout=Layout(HSplit([body, status, prompt]), focused_element=prompt),
            key_bindings=kb,
            full_screen=True,
            style=Style.from_dict({
                "status": "reverse",
                "prompt": "bold",
            }),
        )

    def _status(self):
        link = "CONNECTED" if self.connected else "OFFLINE"
        return (f" {link}  {self.mac}   clock={self.last_time}   "
                f"[type HH:MM + Enter | Ctrl-C quit]")

    def _append(self, text):
        self.lines.append(text)
        self.lines = self.lines[-500:]
        self.log_buf.text = "".join(self.lines)
        self.log_buf.cursor_position = len(self.log_buf.text)

    def _on_enter(self, buf):
        cmd = buf.text.strip()
        buf.reset()
        if not cmd:
            return
        if cmd.lower() in ("q", "quit", "exit"):
            self.app.exit()
            return
        try:
            self.sock.send((cmd + "\r\n").encode())
            self._append(f"[sent] {cmd}\n")
        except OSError as e:
            self._append(f"[send error] {e}\n")

    def _reader(self):
        while True:
            try:
                data = self.sock.recv(64)
            except OSError:
                break
            if not data:
                self.connected = False
                self._append("\n[disconnected]\n")
                break
            m = TIME_RE.search(data)
            if m:
                self.last_time = m.group(1).decode()
            text = data.decode(errors="replace").replace("\r", "")
            self._append(text)
            if self.logfile:
                self.logfile.write(text)
                self.logfile.flush()
            self.app.invalidate()

    def run(self):
        self._append(f"connecting to {self.mac} (channel {self.channel}) ...\n")
        self.sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM,
                                  socket.BTPROTO_RFCOMM)
        try:
            self.sock.connect((self.mac, self.channel))
        except OSError as e:
            print(f"connect failed: {e}\n"
                  "  - paired? (bluetoothctl)\n"
                  "  - disconnect it in the desktop Bluetooth panel first, then retry")
            return 1
        self.connected = True
        self._append("connected.\n")
        threading.Thread(target=self._reader, daemon=True).start()
        self.app.run()
        self.sock.close()
        if self.logfile:
            self.logfile.close()
        return 0


def main():
    ap = argparse.ArgumentParser(description="HC-06 clock TUI over Bluetooth")
    ap.add_argument("mac", nargs="?", default=DEFAULT_MAC, help="HC-06 MAC address")
    ap.add_argument("--channel", type=int, default=CHANNEL, help="RFCOMM channel")
    ap.add_argument("--log", metavar="FILE", help="append the stream to FILE")
    args = ap.parse_args()
    logfile = open(args.log, "a") if args.log else None
    return BTClock(args.mac, args.channel, logfile).run()


if __name__ == "__main__":
    sys.exit(main())
