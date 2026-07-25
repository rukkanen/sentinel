#!/usr/bin/env python3
"""Summarize pinscan output: which digital pins toggled, which ADC pins swung — those
are where the sensors are wired. Usage: pinscan.py [port] [seconds]."""
import sys, time
import serial

BYID = ("/dev/serial/by-id/"
        "usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_sentinel_module-if00-port0")


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else BYID
    dur = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0
    print(f"scanning {port} for {dur:.0f}s — MOVE (radar) and make NOISE (mic)…\n")
    trans, last, high, n = {}, {}, {}, {}
    amin, amax = {}, {}
    samples = 0
    with serial.Serial(port, 115200, timeout=1) as ser:
        end = time.monotonic() + dur
        while time.monotonic() < end:
            line = ser.readline().decode("utf-8", "replace").strip()
            if not line.startswith("D|"):
                continue
            try:
                _, d, _, a = line.split("|", 3)
            except ValueError:
                continue
            samples += 1
            for kv in d.split(","):
                p, v = kv.split("="); v = int(v)
                if p in last and last[p] != v:
                    trans[p] = trans.get(p, 0) + 1
                last[p] = v
                high[p] = high.get(p, 0) + v; n[p] = n.get(p, 0) + 1
            for kv in a.split(","):
                p, v = kv.split("="); v = int(v)
                amin[p] = min(amin.get(p, 99999), v); amax[p] = max(amax.get(p, -1), v)

    print(f"\n{samples} samples.\n--- DIGITAL (transitions = activity) ---")
    for p in sorted(trans, key=lambda k: -trans[k]) or []:
        pct = 100 * high[p] // max(n[p], 1)
        print(f"  GPIO{p}: {trans[p]} transitions, {pct}% high  <-- candidate" if trans[p] > 1 else "")
    if not any(trans.get(p, 0) > 1 for p in trans):
        print("  (no digital pin toggled — radar not on a scanned digital pin, or not driving)")
    print("--- ANALOG ADC1 (range = activity) ---")
    rows = sorted(amax, key=lambda k: -(amax[k] - amin.get(k, 0)))
    for p in rows:
        rng = amax[p] - amin[p]
        flag = "  <-- candidate (swinging)" if rng > 150 else ""
        print(f"  GPIO{p}: {amin[p]}..{amax[p]} (range {rng}){flag}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
