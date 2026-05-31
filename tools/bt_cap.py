#!/usr/bin/env python3
"""bt_cap.py — read & drive the capacitance meter over Bluetooth (RFCOMM/SPP).

The meter streams a measurement when you press its button OR when this tool
sends 'm'. Each run ends with a line like:
    avg dt=467 n=30 min=461 max=472  C=67400 pF
This tool parses that, prints it, and can log calibration points.

Setup: pair the HC-06 in bluetoothctl (PIN 1234/0000). No rfcomm binary needed.

Usage:
    ./bt_cap.py                         # default MAC below
    ./bt_cap.py 98:DA:60:0F:A8:4E
    ./bt_cap.py --log cap_data.csv      # append measurements to a CSV

Commands (type + Enter):
    m            trigger a measurement
    c <pF>       trigger, then log it as a calibration point for a known cap
                 (e.g. "c 10000" for a known 10 nF)
    q            quit
"""

import argparse
import re
import socket
import sys
import threading

DEFAULT_MAC = "98:DA:60:0F:A8:4E"
CHANNEL = 1
RESULT_RE = re.compile(r"avg dt=(\d+).*?C=(\d+)\s*pF")

state = {"last_dt": None, "last_pf": None, "pending_known": None}


def reader(sock, logfile):
    buf = ""
    while True:
        try:
            data = sock.recv(128)
        except OSError:
            break
        if not data:
            print("\n[disconnected]")
            break
        buf += data.decode(errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.strip("\r")
            print(f"  < {line}")
            m = RESULT_RE.search(line)
            if m:
                dt, pf = int(m.group(1)), int(m.group(2))
                state["last_dt"], state["last_pf"] = dt, pf
                print(f"  => dt={dt}  C={pf} pF ({pf/1000:.3f} nF)")
                known = state["pending_known"]
                if known is not None and logfile:
                    logfile.write(f"{known},{dt},{pf}\n")
                    logfile.flush()
                    print(f"  [logged] known={known}pF dt={dt} measured={pf}pF")
                    state["pending_known"] = None


def main():
    ap = argparse.ArgumentParser(description="Capacitance meter over Bluetooth")
    ap.add_argument("mac", nargs="?", default=DEFAULT_MAC)
    ap.add_argument("--channel", type=int, default=CHANNEL)
    ap.add_argument("--log", metavar="FILE", help="append calibration points (known_pF,dt,measured_pF)")
    args = ap.parse_args()

    logfile = None
    if args.log:
        logfile = open(args.log, "a")
        logfile.write("# known_pF,dt_ticks,measured_pF\n")
        logfile.flush()

    print(f"connecting to {args.mac} ...")
    sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
    try:
        sock.connect((args.mac, args.channel))
    except OSError as e:
        print(f"connect failed: {e}\n  - paired? disconnect it in the desktop BT panel, then retry")
        return 1
    print("connected. commands: 'm' measure | 'c <pF>' calibrate-log | 'q' quit\n")

    threading.Thread(target=reader, args=(sock, logfile), daemon=True).start()

    try:
        while True:
            cmd = input().strip()
            if cmd in ("q", "quit", "exit"):
                break
            if cmd == "m" or cmd == "":
                sock.send(b"m\n")
            elif cmd.startswith("c"):
                parts = cmd.split()
                if len(parts) == 2 and parts[1].isdigit():
                    state["pending_known"] = int(parts[1])
                    print(f"  [next result logged as known {parts[1]} pF]")
                    sock.send(b"m\n")
                else:
                    print("  usage: c <known_pF>   e.g. c 10000")
            else:
                print("  ? commands: m | c <pF> | q")
    except (EOFError, KeyboardInterrupt):
        pass
    finally:
        sock.close()
        if logfile:
            logfile.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
