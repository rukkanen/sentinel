# SETUP — reproducible dev/build environment for the Sentinel

**Rule (owner, S64): if it isn't in this file, it didn't happen.** Every install, version,
and privileged step needed to stand up a Sentinel build host lives here, so a second Pi is
copy-paste, not archaeology. `setup.sh` is the executable form; this file is the prose.
These are **dev-host tools, NOT rosbottiNG runtime deps** — the robot's runtime surface
stays stdlib + Flask + pyserial + numpy.

## Host

- Raspberry Pi 4, Ubuntu 24.04 (host `rosbotti`), Python 3.12.3 (system).
- User must be in the `dialout` group (`lapanen` already is): `sudo usermod -aG dialout $USER`.
- No system-wide sudo/apt installs are required beyond stock `python3-venv`.

## Toolchain (installed 2026-07-24, S65)

| what | where | version | why |
|---|---|---|---|
| venv | `~/.venvs/sentinel` | Python 3.12.3 | isolate dev tools from system + yahboom-tools |
| esptool | in venv | **5.3.1** (binary `esptool`; `esptool.py` is the deprecated alias) | rung-0 board prover (`chip-id`/`read-mac`/`flash-id`), flasher of last resort |
| PlatformIO Core | in venv | **6.1.19** | the chosen toolchain (§0e rung 1): hosts Arduino now, ESP-IDF later |
| pyserial | in venv | 3.5 (esptool dep) | Pi-side reader scripts |
| espressif32 platform | pio-managed (`~/.platformio`, ~1.5 GB) | **7.0.1** (pinned in demo/platformio.ini; pulls framework-arduinoespressif32 3.20017 + toolchain-xtensa-esp32 8.4.0+2021r2-patch5) | ESP32 Arduino core + xtensa toolchain |

Exact install command (what `setup.sh` replays):

```sh
python3 -m venv ~/.venvs/sentinel
~/.venvs/sentinel/bin/pip install "esptool==5.3.1" "platformio==6.1.19"
```

## Building + flashing the rung-1 demo

```sh
cd ~/git/sentinel/demo
~/.venvs/sentinel/bin/pio run                                   # build
python3 ../tools/find_sentinel.py                               # identify the Sentinel port SAFELY
~/.venvs/sentinel/bin/pio run -t upload \
    --upload-port /dev/serial/by-id/<sentinel-device>           # flash — EXPLICIT port only
~/.venvs/sentinel/bin/python ../tools/read_demo.py \
    /dev/serial/by-id/<sentinel-device>                         # verify banner + echo
```

**⚠️ Port safety (non-negotiable):** this bus carries the Yahboom robot board
(`usb-1a86_USB_Serial-if00-port0`, ttyUSB0, owned by boardd — **never flash, never open**)
and the LD19 lidar (CP2102). esptool/pio toggle DTR/RTS and hard-reset whatever they touch.
`demo/platformio.ini` pins `upload_port = /dev/sentinel_mcu` (which does not exist until the
udev rule lands) precisely so a portless `pio run -t upload` fails safe instead of guessing.
Always pass the positively-identified by-id path; `tools/find_sentinel.py` refuses the two
known devices by name.

## Proposed udev rule (NOT applied — udev apply is an ask-first gate)

Once the Sentinel's bridge chip + serial are known (rung 0), give it a stable name so it
never races the lidar for ttyUSB numbering. Template (fill from `udevadm info` on the real
device; match on serial, not just VID:PID — a CP2102-based board would collide with the lidar):

```
# /etc/udev/rules.d/99-sentinel.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="XXXX", ATTRS{idProduct}=="XXXX", ATTRS{serial}=="XXXX", SYMLINK+="sentinel_mcu"
```

Then: `sudo udevadm control --reload-rules && sudo udevadm trigger` (owner-run).

## Install log (append-only)

- **2026-07-24 (S65):** created `~/.venvs/sentinel`; `pip install esptool==5.3.1
  platformio==6.1.19` (transitively: pyserial 3.5, cryptography 49.0.0, rich 15.0.0 …
  full freeze reproducible from the two pins). No sudo, no apt, no udev applied.
  First `pio run` of `demo/` downloaded espressif32 7.0.1 into `~/.platformio/` and built
  clean on the Pi 4 in 240 s (RAM 6.6 %, flash 20.6 %).
