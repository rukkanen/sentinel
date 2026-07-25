# SETUP — reproducible dev/build environment for the Sentinel

**Rule (owner, S64): if it isn't in this file, it didn't happen.** Every install, version,
and privileged step needed to stand up a Sentinel build host lives here, so a second Pi is
copy-paste, not archaeology. `setup.sh` is the executable form; this file is the prose.
These are **dev-host tools, NOT rosbottiNG runtime deps** — the robot's runtime surface
stays stdlib + Flask + pyserial + numpy.

## The board — verified identity (2026-07-25, S66, rungs 0+1 GREEN)

| fact | value |
|---|---|
| chip | **ESP32-D0WD, revision v1.0** (classic WROOM-32 silicon; Wi-Fi + BT, dual-core, 240 MHz) |
| MAC | **`4c:11:ae:66:5f:c4`** (stable identity, but only visible via esptool — not to udev) |
| flash | 4 MB, 3.3 V |
| USB bridge | **CP2102** (`10c4:ea60`), USB serial **`0001`** |

⚠️ **The USB bridge COLLIDES with the LD19 lidar** — same VID:PID (`10c4:ea60`) *and* same serial
(`0001`). Consequences, all real:
- `tools/find_sentinel.py`'s by-id filter can't distinguish them (the `..._0001-if00-port0` symlink
  collides — only one exists). Identify the Sentinel by **physical USB port** instead. On rosbotti
  this session it was physical port `1-1.1.3` → `/dev/ttyUSB1`, stable by-path
  `/dev/serial/by-path/platform-fd500000.pcie-pci-0000:01:00.0-usb-0:1.1.3:1.0-port0`. **ttyUSB
  numbering is NOT stable across reboots** (two serial-0001 devices race) — always re-confirm with
  `esptool … chip-id` before flashing, and flash via the **by-path** symlink, never a bare tty.
- For Phase C the `/dev/sentinel_mcu` udev rule **must key on the physical port** (`KERNELS==`), or
  the ESP32's CP2102 serial must be reprogrammed unique (e.g. `cp210x-cfg`). A VID:PID/serial rule
  cannot work. Likewise the rosbotti `selftest/manifest.json` entry can't key on by_id.

## Host

- Raspberry Pi 4, Ubuntu 24.04 (host `rosbotti`), Python 3.12.3 (system).
- User must be in the `dialout` group (`lapanen` already is): `sudo usermod -aG dialout $USER`.
- No system-wide sudo/apt installs are required beyond stock `python3-venv`.

## Toolchain (installed 2026-07-24, S65)

| what | where | version | why |
|---|---|---|---|
| venv | `~/.venvs/sentinel` | Python 3.12.3 | isolate dev tools from system + yahboom-tools |
| esptool | in venv | **5.3.1** (binary `esptool`; `esptool.py` is the deprecated alias) | rung-0 board prover (`chip-id`/`read-mac`/`flash-id`), flasher of last resort |
| PlatformIO Core | in venv | **6.1.19** | the **demo** build tool (§0e rung 1); whether it stays the real-firmware tool is the §0f open decision — see below |
| pyserial | in venv | 3.5 (esptool dep) | Pi-side reader scripts |
| espressif32 platform | pio-managed (`~/.platformio`, ~1.5 GB) | **7.0.1** (pinned in demo/platformio.ini; pulls framework-arduinoespressif32 3.20017 + toolchain-xtensa-esp32 8.4.0+2021r2-patch5) | ESP32 Arduino core + xtensa toolchain |

Exact install command (what `setup.sh` replays):

```sh
python3 -m venv ~/.venvs/sentinel
~/.venvs/sentinel/bin/pip install "esptool==5.3.1" "platformio==6.1.19"
```

### ⚠️ TWO pio Cores exist — which one is truth? (OPEN decision, prompt 14 §0f)

There are **two PlatformIO Core installs** and it matters:
1. **the pinned venv** `~/.venvs/sentinel/bin/pio` (this file, 6.1.19) — the reproducible one.
2. **the VS Code PlatformIO IDE extension's bundled core**, `~/.platformio/penv/bin/pio`
   (extension `platformio.platformio-ide-3.3.4`, 6.1.19 *today*, but it auto-updates itself).

They **share** the data dir `~/.platformio/` (platforms/frameworks/toolchains, ~1.6 GB), so the
*libs* are not duplicated — only the core `pio` executable is. **Risk:** the two cores drift in
version (the extension bumps itself; the venv stays pinned) and then the VS Code **Build** button
and this file's `pio` are different programs → non-reproducible builds. **The VS Code extension is
optional** — no rung needs it; `esptool` alone does rung 0, `pio` + platform does rungs 1–2.

**RESOLVED (S66, owner asked Claude to choose): the pinned venv `~/.venvs/sentinel` is the ONE
authoritative core.** Use `~/.venvs/sentinel/bin/pio` for everything that must be reproducible.
The VS Code PlatformIO extension is **optional** (no rung needs it) — if kept, point it at the venv
(`"platformio-ide.useBuiltinPIOCore": false`, `"platformio-ide.customPyPath":
"~/.venvs/sentinel/bin/python"`) so the Build button == the CLI; otherwise ignore or remove it.

**RESOLVED (S66): PlatformIO stays the build/test/flash tool** — chosen for clean, Claude-led,
hands-off, spec→RED→GREEN dev because one tool gives a host `native` test env + cross-build + flash
from one pinned file (rosbottiNG prompt 14 §0f Resolution). The **framework** (Arduino vs ESP-IDF,
both under pio) is the one piece still deferred to the Phase B spec; pio's Arduino core is 2.0.17 →
IDF 4.4-era (has every robustness lever; currency, not capability). Lean: Arduino-under-pio to
start, portable logic host-tested, escalate to `framework=espidf` only if a lever demands it.

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

## Fixing the lidar collision: reprogram the CP2102 serial → `sentinel_module` (OWNER-RUN)

The Sentinel's CP2102 ships with serial `0001` — identical to the LD19 lidar (see the board
identity table above), so udev/by-id can't tell them apart. The clean, location-independent fix
(owner-chosen, S66) is to give the ESP32's bridge a **unique serial**, then a normal by-id udev
rule works and the lidar stops being ambiguous. All steps below need `sudo` (privileged USB /
udev) and are **owner-run** — Claude prepares, the owner executes.

**0. Tools** (once): `./setup.sh cp210x` — installs `pyusb`+`hexdump` in the venv and fetches the
pinned reprogram tool to `vendor/cp210x-program` (VCTLabs/cp210x-program @ 927ed26, AN721, LGPL).

**1. Free the port + confirm the target.** Nothing may hold the ESP32's tty during the write —
stop whatever has it (the dashboard `app.py` grabs it via the colliding by-id when the lidar is
absent). Then positively confirm it's the Sentinel, not the lidar:
```
python3 tools/find_sentinel.py                 # picks the ESP32; note its /dev/ttyUSBn + phys-port
lsusb | grep 10c4:ea60                          # note Bus/Device, e.g. "Bus 001 Device 019"
~/.venvs/sentinel/bin/esptool --port <by-path> chip-id   # MUST report MAC 4c:11:ae:66:5f:c4
```
Ideally do this while the lidar is off-bus (only ONE `10c4:ea60` present) — zero chance of hitting
the wrong chip. The `-m <bus>/<dev>` selector below targets that exact device regardless.

**2. Back up the whole EEPROM first** (restore path if anything goes wrong — your only board):
```
sudo ~/.venvs/sentinel/bin/python vendor/cp210x-program/cp210x-program \
     --read-cp210x -m <bus>/<dev> -f sentinel.eeprom-backup.hex
```

**3. Write the new serial + reset so it re-enumerates:**
```
sudo ~/.venvs/sentinel/bin/python vendor/cp210x-program/cp210x-program \
     --write-cp210x -m <bus>/<dev> --set-serial-number sentinel_module --reset-device
```

**4. Verify** (replug if needed): `ls -l /dev/serial/by-id/` now shows
`…CP2102…_sentinel_module-if00-port0`, and `tools/find_sentinel.py` names it by a unique serial.
(Restore from the backup with `--write-cp210x -F sentinel.eeprom-backup.hex` if ever needed.)

## udev rule → `/dev/sentinel_mcu` (OWNER-RUN, after step 4)

With a unique serial the rule keys on `serial` cleanly (won't match the lidar's `0001`):
```
# /etc/udev/rules.d/99-sentinel.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", ATTRS{serial}=="sentinel_module", SYMLINK+="sentinel_mcu"
```
Then: `sudo udevadm control --reload-rules && sudo udevadm trigger`. After this, flash/monitor via
the stable `/dev/sentinel_mcu`. (Bigger picture: there is no central serial-lease manager for
non-board devices — the lidar/Sentinel are opened ad-hoc by the dashboard; a unified device
manager is a separate proposed prompt. Until then, `/dev/sentinel_mcu` + this rule is the fix.)

## Install log (append-only)

- **2026-07-24 (S65):** created `~/.venvs/sentinel`; `pip install esptool==5.3.1
  platformio==6.1.19` (transitively: pyserial 3.5, cryptography 49.0.0, rich 15.0.0 …
  full freeze reproducible from the two pins). No sudo, no apt, no udev applied.
  First `pio run` of `demo/` downloaded espressif32 7.0.1 into `~/.platformio/` and built
  clean on the Pi 4 in 240 s (RAM 6.6 %, flash 20.6 %).
- **2026-07-25 (S66):** `pip install pyusb hexdump` added to the venv (reprogram-tool deps; no
  apt — libusb-1.0 runtime already on the OS). `setup.sh cp210x` fetches VCTLabs/cp210x-program
  @ 927ed26 to `vendor/`. The CP2102 serial reprogram + `/dev/sentinel_mcu` udev rule are
  **staged but NOT yet run** (owner runs the sudo steps above).
