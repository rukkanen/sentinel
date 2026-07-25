#!/usr/bin/env python3
"""Rung-2 verifier: read SENT-LINK frames, validate CRC, pretty-print sensor events.

Usage: read_rung2.py [port] [seconds]   (defaults: sentinel_module by-id, 20 s)
Frame: <ver><type><seq>|<json>|<crc16>   crc = CRC-16/CCITT-FALSE over "<...>|<json>".
Needs pyserial (sentinel venv). This is the SENT-LINK seed parser — the real Pi
bridge (spec SENT-100) is built properly in Phase C.
"""
import sys, time
import serial

BYID = ("/dev/serial/by-id/"
        "usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_sentinel_module-if00-port0")


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else BYID
    dur = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0
    print(f"reading {port} for {dur:.0f}s — type / move and watch the events…\n")
    ok = bad = radar = sound = 0
    with serial.Serial(port, 115200, timeout=1) as ser:
        end = time.monotonic() + dur
        while time.monotonic() < end:
            line = ser.readline().decode("utf-8", "replace").strip()
            if not line:
                continue
            try:
                head, crcs = line.rsplit("|", 1)
                valid = f"{crc16_ccitt(head.encode()):04X}" == crcs.upper()
            except ValueError:
                head, valid = line, False
            ok += valid
            bad += not valid
            if '"s":"radar"' in line:
                radar += 1
            if '"s":"sound"' in line:
                sound += 1
            print(f"[{'ok ' if valid else 'BAD'}] {line}")
    print(f"\nframes: {ok} valid, {bad} bad · radar events: {radar} · sound events: {sound}")
    return 0 if ok and not bad else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
