#!/usr/bin/env python3
"""Find the Sentinel's serial port safely — never guess on this bus.

The robot's USB bus carries other serial devices that must NEVER be reset/flashed
by esptool/pio (esptool toggles DTR/RTS and hard-resets whatever it opens):
  - the Yahboom YB-ERF01 robot board (CH340 1a86:7523, owned by boardd)
  - the LD19 lidar (CP2102 10c4:ea60)

⚠️ THE COLLISION (verified S66, 2026-07-25): the Sentinel ESP32 is a **CP2102** board
with USB serial **0001** — IDENTICAL VID:PID *and* serial to the lidar. So /dev/serial
/by-id CANNOT tell them apart (the by-id name collides — only one symlink exists), and
neither can udev by attributes. The only robust discriminators are the **physical USB
port** and **who holds the port**. This tool therefore enumerates raw ttyUSB via sysfs,
not by-id, and reports every serial device with its chip / physical port / holder so a
human (or the next step, esptool chip-id) can pick the Sentinel with certainty.

stdlib only; run with any python3.
"""

import argparse
import os
import sys
import time
from pathlib import Path

SYS_TTY = Path("/sys/class/tty")

CHIPS = {
    ("1a86", "7523"): "CH340",
    ("10c4", "ea60"): "CP2102",
    ("1a86", "55d4"): "CH9102",
}
# Positively-known devices, by (vid, pid) — used only to LABEL, never to auto-pick a
# flash target. The lidar shares CP2102/0001 with the Sentinel, so vid:pid alone can't
# exclude it: a CP2102 is "lidar OR sentinel", disambiguated by port + holder below.
BOARD = ("1a86", "7523")   # robot board — NEVER touch
CP2102 = ("10c4", "ea60")  # lidar AND sentinel both live here


def _read(p):
    try:
        return p.read_text().strip()
    except OSError:
        return None


def usb_ancestor(tty_name):
    """Walk up from /sys/class/tty/<name>/device to the USB device dir (has idVendor).
    Returns (vid, pid, serial, port) — port is the stable physical id, e.g. '1-1.1.3'."""
    node = (SYS_TTY / tty_name / "device").resolve()
    for _ in range(8):
        if (node / "idVendor").exists():
            return (
                _read(node / "idVendor"),
                _read(node / "idProduct"),
                _read(node / "serial"),
                node.name,  # e.g. '1-1.1.3' — the physical port path
            )
        node = node.parent
    return (None, None, None, None)


def holder(dev):
    """Best-effort: which process (of ours) holds /dev/ttyUSBn. Same-user procs only."""
    for proc in Path("/proc").iterdir():
        if not proc.name.isdigit():
            continue
        try:
            for fd in (proc / "fd").iterdir():
                try:
                    if os.readlink(fd) == dev:
                        return f"{_read(proc / 'comm') or '?'}(PID {proc.name})"
                except OSError:
                    continue
        except (PermissionError, FileNotFoundError, NotADirectoryError):
            continue
    return None


def scan():
    """List every ttyUSB with chip / serial / physical port / holder / classification."""
    rows = []
    for tty in sorted(SYS_TTY.glob("ttyUSB*")):
        name = tty.name
        dev = f"/dev/{name}"
        vid, pid, serial, port = usb_ancestor(name)
        chip = CHIPS.get((vid, pid), f"{vid}:{pid}")
        held = holder(dev)
        if (vid, pid) == BOARD:
            klass = "board"      # robot board — never touch
        elif (vid, pid) == CP2102:
            klass = "cp2102"     # lidar OR sentinel — decide by port/holder
        else:
            klass = "candidate"  # e.g. native-USB ESP32 303a / CH9102 — strong Sentinel
        rows.append(dict(dev=dev, chip=chip, vid=vid, pid=pid, serial=serial,
                         port=port, held=held, klass=klass, bypath=by_path_for(port)))
    return rows


def by_path_for(port):
    """The stable /dev/serial/by-path symlink for a physical port (flash via THIS)."""
    d = Path("/dev/serial/by-path")
    if not d.is_dir() or not port:
        return None
    for link in d.iterdir():
        try:
            if f"-usb-0:{port.split('-', 1)[1]}:" in link.name and "port0" in link.name:
                return str(link)
        except (IndexError, OSError):
            continue
    return None


def report(rows):
    if not rows:
        print("no ttyUSB serial devices on the bus.")
        return 1
    print(f"{'dev':<13} {'chip':<10} {'serial':<8} {'phys-port':<10} holder")
    for r in rows:
        tag = {"board": "  [robot board — DO NOT TOUCH]",
               "cp2102": "  [lidar OR Sentinel]",
               "candidate": "  [Sentinel candidate]"}[r["klass"]]
        print(f"{r['dev']:<13} {r['chip']:<10} {str(r['serial']):<8} "
              f"{str(r['port']):<10} {r['held'] or '-'}{tag}")

    # Pick the best Sentinel candidate.
    natives = [r for r in rows if r["klass"] == "candidate"]
    cps = [r for r in rows if r["klass"] == "cp2102"]
    unheld_cps = [r for r in cps if not r["held"]]

    print()
    if natives:
        pick = natives[0]
        print(f"→ Sentinel = {pick['dev']} (distinct chip {pick['chip']}).")
    elif len(cps) >= 2 and len(unheld_cps) == 1:
        pick = unheld_cps[0]
        print(f"→ Sentinel = {pick['dev']} — the ONE unheld CP2102 "
              f"(the other CP2102 is the lidar, currently held).")
    elif len(cps) == 1:
        pick = cps[0]
        print(f"⚠ one CP2102 only ({pick['dev']}). If the lidar is unplugged this is the "
              f"Sentinel; if plugged, this may BE the lidar. Confirm with chip-id below.")
    elif unheld_cps:
        pick = unheld_cps[0]
        print(f"⚠ multiple unheld CP2102 — cannot auto-pick. Candidates: "
              f"{', '.join(r['dev'] for r in unheld_cps)}. Confirm each with chip-id.")
    else:
        print("no Sentinel candidate (only the board, or every CP2102 is held).")
        return 1

    tgt = pick["bypath"] or pick["dev"]
    print("  CONFIRM it is the ESP32 before flashing (safe, resets only that port):")
    print(f"    ~/.venvs/sentinel/bin/esptool --port {tgt} chip-id")
    print("  flash via the by-path (stable if ttyUSB numbers shuffle), never a bare tty.")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--watch", action="store_true",
                    help="poll until the set of ttyUSB devices changes (plug in while running)")
    args = ap.parse_args()

    if not args.watch:
        return report(scan())

    base = {r["dev"]: r["port"] for r in scan()}
    print(f"watching for a serial-device change ({len(base)} present; Ctrl-C to stop)…")
    while True:
        time.sleep(1)
        now = scan()
        keys = {r["dev"]: r["port"] for r in now}
        if keys != base:
            print("↳ bus changed:")
            return report(now)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
