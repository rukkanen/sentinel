#!/usr/bin/env bash
# Stand up the Sentinel dev/build environment on a fresh Pi. Idempotent.
# Prose companion: SETUP.md — the rule is "if it isn't in SETUP.md, it didn't happen";
# keep the two in sync (versions pinned here are documented there).
set -euo pipefail

VENV="$HOME/.venvs/sentinel"
ESPTOOL_VER="5.3.1"
PIO_VER="6.1.19"

command -v python3 >/dev/null || { echo "need python3 + python3-venv (sudo apt install python3-venv)"; exit 1; }
python3 -c 'import venv' 2>/dev/null || { echo "need python3-venv (sudo apt install python3-venv)"; exit 1; }

[ -d "$VENV" ] || python3 -m venv "$VENV"
"$VENV/bin/pip" install --quiet --upgrade pip
"$VENV/bin/pip" install --quiet "esptool==$ESPTOOL_VER" "platformio==$PIO_VER"

# dialout group is needed to open /dev/ttyUSB* — apt/usermod are owner-run steps.
id -nG | grep -qw dialout || echo "WARNING: $USER not in dialout — run: sudo usermod -aG dialout $USER (then re-login)"

echo "esptool: $("$VENV/bin/esptool" version | tail -1)"
echo "pio:     $("$VENV/bin/pio" --version)"
echo "OK — build the rung-1 demo with:"
echo "  cd demo && $VENV/bin/pio run"
