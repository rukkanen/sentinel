# Sentinel TODO — Phase 0 ladder (rosbottiNG docs/prompts/14_sentinel_bringup.md is the job)

Status legend: ✅ done · 🔶 ready/blocked · ⬜ open

- ✅ **Rung 0 — aliveness. GREEN (2026-07-25, S66, on rosbotti).** The S65 blocker was the
  cable (charge-only) + a powered hub mid-power-loss, not the board. With a data cable the
  ESP32 enumerated. `esptool chip-id` synced: **ESP32-D0WD rev v1.0, MAC 4c:11:ae:66:5f:c4,
  4 MB flash**. It's a **CP2102 board that collides with the lidar** (VID:PID + serial 0001) —
  identify by physical port, see SETUP.md.
- ✅ **Rung 1 — flash → boot → talk. GREEN (S66).** `pio run -t upload` (via by-path
  `…usb-0:1.1.3:1.0-port0`) flashed `demo/`; `tools/read_demo.py` verified banner + echo.
- ⬜ **Fix `tools/find_sentinel.py`** — it missed the ESP32 (by-id collision). Make it
  enumerate raw ttyUSB via sysfs and flag CP2102 collisions by physical port + holder.
- 🔶 **Rung 1 — flash → boot → talk.** Demo firmware written (`demo/`: blink + banner +
  echo) and toolchain stood up (SETUP.md); flash + `tools/read_demo.py` the moment rung 0
  passes.
- ⬜ **§0f — toolchain finding-out + owner gate** (owner-requested 2026-07-25). Part 1: install
  audit — TWO pio Cores exist (pinned venv vs the VS Code extension's `~/.platformio/penv`,
  sharing one lib dir); pick which is truth (see SETUP.md). Part 2: is PlatformIO still the right
  build tool, or build the bins with ESP-IDF-native / arduino-cli? Coupled to the Phase B
  Arduino-vs-IDF choice; recommendation → owner gate before Phase C.
- ⬜ **Rung 2 — one sensor, one framed event** (radar GPIO12 or sound GPIO14; SENT-LINK seed
  frame: prefix+seq+payload+CRC).
- ⬜ **Phase B — `sentinel_spec.md` + OWNER GATE** (do not implement past the demo until the
  owner accepts the spec). Raise: ESP-IDF vs Arduino; USB-hub power ≠ two-tier; flaky-cascade
  placement.
- ⬜ **Phase C — real firmware + Pi bridge, spec→RED→GREEN** (firmware here; bridge, selftest
  manifest entry, udev proposal in rosbottiNG).

Legacy sketch (`src/`, `include/`, `platformio.ini` at root) = statement of intent only
(owner S64); mined in prompt 14 §0c, will be replaced in Phase C, kept for git history.
