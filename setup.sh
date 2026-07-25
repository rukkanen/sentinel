#!/usr/bin/env bash
# Stand up the Sentinel dev/build environment on a fresh Pi. Idempotent.
# Prose companion: SETUP.md — the rule is "if it isn't in SETUP.md, it didn't happen";
# keep the two in sync (versions pinned here are documented there).
#
# Usage:
#   ./setup.sh            # core toolchain (esptool + PlatformIO + USB-reprogram deps)
#   ./setup.sh cp210x     # ALSO fetch the pinned CP210x reprogram tool (needs network)
set -euo pipefail

VENV="$HOME/.venvs/sentinel"
ESPTOOL_VER="5.3.1"
PIO_VER="6.1.19"
# CP210x USB-serial reprogram tool (VCTLabs/cp210x-program) — pinned commit for reproducibility.
CP210X_SHA="927ed264fe4a3aeb360cd0e7862bfb19b8c2e6bb"
VENDOR="$HOME/git/sentinel/vendor"

command -v python3 >/dev/null || { echo "need python3 + python3-venv (sudo apt install python3-venv)"; exit 1; }
python3 -c 'import venv' 2>/dev/null || { echo "need python3-venv (sudo apt install python3-venv)"; exit 1; }

[ -d "$VENV" ] || python3 -m venv "$VENV"
"$VENV/bin/pip" install --quiet --upgrade pip
# esptool + PlatformIO = build/flash. pyusb + hexdump = deps of the CP210x reprogram tool
# (below) and of tools/find_sentinel.py's verify path. libusb-1.0 RUNTIME must exist
# (it ships with the OS: /lib/*/libusb-1.0.so.0) — no -dev headers needed for pyusb.
"$VENV/bin/pip" install --quiet "esptool==$ESPTOOL_VER" "platformio==$PIO_VER" pyusb hexdump

# dialout group is needed to open /dev/ttyUSB* — apt/usermod are owner-run steps.
id -nG | grep -qw dialout || echo "WARNING: $USER not in dialout — run: sudo usermod -aG dialout $USER (then re-login)"

# Host C++ compiler for the PlatformIO `native` unit tests (firmware/ Phase C, spec SENT-160).
# pio bundles the ESP32 (xtensa) toolchain but host tests use the SYSTEM compiler.
command -v g++ >/dev/null || echo "NOTE: no host C++ compiler — for 'pio test -e native' run: sudo apt install build-essential"

# --- optional: fetch the CP210x reprogram tool (only when asked; needs network) ----------
if [ "${1:-}" = "cp210x" ]; then
  mkdir -p "$VENDOR"
  if [ ! -x "$VENDOR/cp210x-program/cp210x-program" ]; then
    echo "fetching cp210x-program @ ${CP210X_SHA:0:12} …"
    tmp="$(mktemp -d)"
    curl -sSL "https://codeload.github.com/VCTLabs/cp210x-program/tar.gz/${CP210X_SHA}" -o "$tmp/c.tgz"
    tar -xzf "$tmp/c.tgz" -C "$tmp"
    rm -rf "$VENDOR/cp210x-program"
    mv "$tmp/cp210x-program-${CP210X_SHA}" "$VENDOR/cp210x-program"
    rm -rf "$tmp"
  fi
  # NB: the tool's cp210x-program is a symlink into scripts/, so it must run with the repo root
  # on PYTHONPATH or `import cp210x` fails. SETUP.md documents the exact sudo invocation.
  echo "cp210x-program ready: $VENDOR/cp210x-program"
  echo "  reprogram (needs sudo; see SETUP.md): cd $VENDOR/cp210x-program && \\"
  echo "    sudo env PYTHONPATH=\"\$PWD\" $VENV/bin/python cp210x-program --write-cp210x -m <bus>/<dev> --set-serial-number sentinel_module --reset-device"
else
  echo "note: to fetch the CP210x reprogram tool, run: ./setup.sh cp210x"
fi

echo "esptool: $("$VENV/bin/esptool" version | tail -1)"
echo "pio:     $("$VENV/bin/pio" --version)"
echo "OK — build the rung-1 demo with:"
echo "  cd demo && $VENV/bin/pio run"
