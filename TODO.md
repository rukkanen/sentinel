# Sentinel TODO — Phase 0 ladder (rosbottiNG docs/prompts/14_sentinel_bringup.md is the job)

Status legend: ✅ done · 🔶 ready/blocked · ⬜ open

- 🔶 **Rung 0 — aliveness. DO IT ON THE DELL (lapadel)** — runbook = rosbottiNG
  `docs/prompts/14_sentinel_bringup.md` §0e (2026-07-25; the standalone prompt 15 was folded
  back into 14). On rosbotti the WROOM never enumerated across ALL logged boots (S64 + S65:
  two different MCUs, zero kernel USB events; solid red power LED = VBUS ok, no data →
  charge-only cable / dead hub port suspected; the robot's powered-hub cascade is hard-down
  and survives reboot). On the Dell: `./setup.sh`, then direct port + known-data cable →
  `tools/find_sentinel.py --watch` → `esptool chip-id` decides board-alive vs dead; record
  VID:PID / by-id / serial / MAC for the eventual udev rule + rosbotti selftest manifest.
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
