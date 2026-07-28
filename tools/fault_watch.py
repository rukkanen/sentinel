#!/usr/bin/env python3
"""fault_watch — live wiring-fault canary for the sentinel_module.

Run this WHILE connecting/powering hardware. It watches the guard firmware's heartbeat and
screams the instant anything goes wrong:
  • USB DROP   — the device vanishes from the bus  → a 5V↔GND short tripped the Pi's USB
                 over-current. CUT POWER.
  • BROWNOUT   — the guard reboots printing reset=BROWNOUT → a short sagged the 3.3 V rail.
  • RESET      — any unexpected reboot mid-run.
  • HB STALL   — heartbeat stops for >1 s while still connected → the MCU hung/crashed.

stdlib + pyserial. Ctrl-C to stop.  Usage: fault_watch.py [seconds]  (default: run until stopped)
"""
import os
import subprocess
import sys
import time

import serial

BYID = ("/dev/serial/by-id/"
        "usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_sentinel_module-if00-port0")
HB_STALL_S = 1.0
BAUD = 115200


def present() -> bool:
    return os.path.exists(BYID)


def kernel_usb_tail() -> str:
    try:
        out = subprocess.run(["journalctl", "-k", "--no-pager", "--since", "10 sec ago"],
                             capture_output=True, text=True, timeout=5).stdout
        hits = [l for l in out.splitlines()
                if any(k in l.lower() for k in ("over-current", "overcurrent", "disconnect",
                                                "cannot enable", "unable to enumerate", "1-1"))]
        return "\n    ".join(hits[-4:])
    except Exception:
        return ""


def alert(msg: str):
    print(f"\n🚨🚨  {msg}  🚨🚨", flush=True)


def main() -> int:
    dur = float(sys.argv[1]) if len(sys.argv) > 1 else None
    end = (time.monotonic() + dur) if dur else None
    print(f"fault_watch — watching {os.path.basename(BYID)}")
    print("Baseline:", "device PRESENT" if present() else "device ABSENT (plug it in first)")
    print("→ connect/power your wiring now. Alerts print in real time.\n")

    ser = None
    last_hb = time.monotonic()
    boots = 0
    last_ok_print = 0.0

    while end is None or time.monotonic() < end:
        # 1) presence — the clearest short signal
        if not present():
            alert("USB DROP — device vanished from the bus → likely a SHORT / over-current. CUT POWER.")
            k = kernel_usb_tail()
            if k:
                print("    kernel:", k, flush=True)
            if ser:
                try: ser.close()
                except Exception: pass
                ser = None
            time.sleep(0.5)
            continue

        # 2) serial — heartbeat / brownout / reset
        try:
            if ser is None:
                ser = serial.Serial(BYID, BAUD, timeout=0.2)
                print("… serial open", flush=True)
            line = ser.readline().decode("utf-8", "replace").strip()
        except serial.SerialException:
            alert("SERIAL LOST — port dropped mid-read → short/reset. CUT POWER if unexpected.")
            ser = None
            time.sleep(0.3)
            continue

        now = time.monotonic()
        if line:
            if "HB" in line:
                last_hb = now
            if "reset=BROWNOUT" in line or "Brownout detector" in line:
                alert("BROWNOUT — the 3.3 V rail sagged (short / overload). CUT POWER.")
                print("   ", line, flush=True)
            elif line.startswith("GUARD boot"):
                boots += 1
                if boots > 1:
                    alert(f"UNEXPECTED RESET — the board rebooted mid-run.  ({line})")
                else:
                    print("   ", line, "(initial)", flush=True)

        # 3) heartbeat stall
        if now - last_hb > HB_STALL_S:
            alert(f"HEARTBEAT STALL — no HB for {now - last_hb:.1f}s → MCU hung/crashed.")
            last_hb = now  # avoid spamming; re-arm

        # 4) periodic all-clear so you know it's alive
        if now - last_ok_print > 3.0:
            last_ok_print = now
            print(f"  ok · present · HB fresh ({now - last_hb:.1f}s ago) · boots={boots}", flush=True)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nfault_watch stopped.")
        sys.exit(130)
