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
- ✅ **§0f — toolchain finding-out (S66, owner asked Claude to choose).** RESOLVED: PlatformIO
  stays as build/test/flash tool (host `native` test env + cross-build + flash from one pinned
  file = best for clean Claude-led hands-off spec→RED→GREEN); pinned venv is the authoritative
  core, VS Code extension optional. Framework (Arduino vs ESP-IDF, both under pio) still deferred
  to the Phase B spec — see SETUP.md + rosbottiNG prompt 14 §0f Resolution.
- 🔶 **Collision permanent fix — STAGED, owner to run (S66).** Chosen: reprogram the CP2102 serial
  to `sentinel_module` (location-independent; a normal `/dev/sentinel_mcu` by-id udev rule then
  works and the lidar stops being ambiguous). ✅ ESP32 moved to a direct Pi port (1-1.2). Tooling
  staged: `./setup.sh cp210x` + pyusb/hexdump; full procedure (backup → write → verify → udev)
  in SETUP.md. Owner runs the sudo steps. Not blocking rung 2.
- ⬜ **Centralized device-lease manager → written up as rosbottiNG prompt 16** (S66):
  `docs/prompts/16_device_manager.md` — generalize the boardd broker to own every serial device
  (stable name + exclusive lease + presence), boardd left untouched. Spec-first, owner-gated,
  D4 rigor. The Sentinel bridge (prompt 14 Phase C) becomes its first new consumer.
- ⬜ **GREENFIELD (owner, S66): the real firmware is written from scratch — the old `src/`
  sketch goes in the bin, not preserved.** The proper iterated plan = the Phase B spec, written
  with the owner (hands-off-code). Old `src/`, `include/`, root `platformio.ini` kept only for
  git history until Phase C starts, then removed.
- ⬜ **Rung 2 — one sensor, one framed event** (radar GPIO12 or sound GPIO14; SENT-LINK seed
  frame: prefix+seq+payload+CRC).
- ⬜ **Phase B — `sentinel_spec.md` + OWNER GATE** (do not implement past the demo until the
  owner accepts the spec). Raise: ESP-IDF vs Arduino; USB-hub power ≠ two-tier; flaky-cascade
  placement.
- ⬜ **Phase C — real firmware + Pi bridge, spec→RED→GREEN** (firmware here; bridge, selftest
  manifest entry, udev proposal in rosbottiNG).

Legacy sketch (`src/`, `include/`, `platformio.ini` at root) = statement of intent only
(owner S64); mined in prompt 14 §0c, will be replaced in Phase C, kept for git history.
