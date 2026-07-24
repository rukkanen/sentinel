#!/usr/bin/env python3
"""Find the Sentinel's serial port safely — never guess on this bus.

The robot's USB bus carries two other serial devices that must NEVER be touched
by esptool/pio (esptool toggles DTR/RTS and auto-resets the target):
  - the Yahboom YB-ERF01 robot board (CH340, owned by boardd)
  - the LD19 lidar (CP2102)

This tool lists /dev/serial/by-id entries, filters out those two known devices,
and reports what is left as the Sentinel candidate. With --watch it polls until
a NEW device appears (plug the ESP32 in while it runs).

stdlib only; run with any python3.
"""

import argparse
import sys
import time
from pathlib import Path

BY_ID = Path("/dev/serial/by-id")

# Known devices on this robot (selftest/manifest.json in rosbottiNG is the truth).
# NB: a CP2102-based ESP32 board would produce a SECOND CP2102 entry — the lidar
# is only excluded by its exact serial-bearing by-id name, not by chip.
KNOWN = {
    "usb-1a86_USB_Serial-if00-port0": "YB-ERF01 robot board (DO NOT TOUCH)",
    "usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0":
        "LD19 lidar (DO NOT TOUCH)",
}


def snapshot() -> dict:
    if not BY_ID.is_dir():
        return {}
    return {p.name: str(p.resolve()) for p in sorted(BY_ID.iterdir())}


def classify(devs: dict) -> list:
    return [(name, tty) for name, tty in devs.items() if name not in KNOWN]


def report(devs: dict) -> int:
    for name, tty in devs.items():
        if name in KNOWN:
            print(f"  known : {name} -> {tty}  [{KNOWN[name]}]")
    candidates = classify(devs)
    if not candidates:
        print("no Sentinel candidate on the bus (only known devices, or none).")
        return 1
    for name, tty in candidates:
        print(f"  CANDIDATE: /dev/serial/by-id/{name} -> {tty}")
    print("\nuse the by-id path (stable across renumbering), e.g.:")
    name = candidates[0][0]
    print(f"  ~/.venvs/sentinel/bin/esptool --port /dev/serial/by-id/{name} chip-id")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--watch", action="store_true",
                    help="poll until a new serial device appears")
    args = ap.parse_args()

    if not args.watch:
        return report(snapshot())

    base = snapshot()
    print(f"watching for a new serial device ({len(base)} present; Ctrl-C to stop)…")
    while True:
        time.sleep(1)
        now = snapshot()
        new = {n: t for n, t in now.items() if n not in base}
        gone = [n for n in base if n not in now]
        for n in gone:
            print(f"  gone: {n}")
        if new:
            print("NEW device(s):")
            return report(now)
        base = {n: t for n, t in now.items()}


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
