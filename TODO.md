# Sentinel TODO — Phase 0 ladder (rosbottiNG docs/prompts/14_sentinel_bringup.md is the job)

Status legend: ✅ done · 🔶 ready/blocked · ⬜ open

- 🔶 **Rung 0 — aliveness.** BLOCKED ON HARDWARE: the WROOM has never enumerated on the Pi
  (S64 + S65: no new serial device at any point, even with the Realtek cascade up — see
  rosbottiNG diary S65). Owner: check power LED, swap to a known-data USB cable, try a
  direct Pi port; is it a dev board (onboard USB-UART) or a bare module (needs an external
  adapter)? Once ANY new tty appears: `tools/find_sentinel.py` → `esptool chip-id` decides
  board-alive vs dead.
- 🔶 **Rung 1 — flash → boot → talk.** Demo firmware written (`demo/`: blink + banner +
  echo) and toolchain stood up (SETUP.md); flash + `tools/read_demo.py` the moment rung 0
  passes.
- ⬜ **Rung 2 — one sensor, one framed event** (radar GPIO12 or sound GPIO14; SENT-LINK seed
  frame: prefix+seq+payload+CRC).
- ⬜ **Phase B — `sentinel_spec.md` + OWNER GATE** (do not implement past the demo until the
  owner accepts the spec). Raise: ESP-IDF vs Arduino; USB-hub power ≠ two-tier; flaky-cascade
  placement.
- ⬜ **Phase C — real firmware + Pi bridge, spec→RED→GREEN** (firmware here; bridge, selftest
  manifest entry, udev proposal in rosbottiNG).

Legacy sketch (`src/`, `include/`, `platformio.ini` at root) = statement of intent only
(owner S64); mined in prompt 14 §0c, will be replaced in Phase C, kept for git history.
