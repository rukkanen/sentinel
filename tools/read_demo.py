#!/usr/bin/env python3
"""Rung-1 verifier: read the demo banner from the Sentinel and test the echo.

Usage:
  ~/.venvs/sentinel/bin/python tools/read_demo.py /dev/serial/by-id/<sentinel-device>

Refuses the robot board / lidar ports outright. Needs pyserial (present in the
sentinel venv via esptool).
"""

import sys
import time

import serial  # pyserial

FORBIDDEN = ("usb-1a86_USB_Serial-if00-port0",
             "usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0")
BAUD = 115200


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    port = sys.argv[1]
    if any(f in port for f in FORBIDDEN):
        print(f"REFUSED: {port} is the robot board or the lidar, not the Sentinel.")
        return 3

    print(f"opening {port} @ {BAUD} (10 s listen, then echo test)…")
    with serial.Serial(port, BAUD, timeout=1) as ser:
        deadline = time.monotonic() + 10
        banner_seen = False
        while time.monotonic() < deadline:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                print(f"  << {line}")
                if line.startswith("SENTINEL demo v1"):
                    banner_seen = True

        probe = "hello-from-the-pi"
        print(f"  >> {probe}")
        ser.write((probe + "\n").encode())
        echo_seen = False
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not echo_seen:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                print(f"  << {line}")
                echo_seen = line == f"echo:{probe}"

    print(f"\nbanner: {'OK' if banner_seen else 'MISSING'}   "
          f"echo: {'OK' if echo_seen else 'MISSING'}")
    return 0 if (banner_seen and echo_seen) else 1


if __name__ == "__main__":
    sys.exit(main())
