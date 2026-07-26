# IR remote button map — "SPECIAL FOR MP3" NEC remote (sentinel_module)

The generic car-MP3 IR remote (VS1838B receiver on **GPIO4**, NEC protocol, address `0x00`).
Captured live 2026-07-27 (S69): **10 of 21 codes verified on-hardware, and every one matches the
standard map with zero mismatches** — so the remaining 11 are the well-documented standard codes
(high confidence; verify a specific button before you bind a command to it).

| button (as printed) | NEC `cmd` | source |
|---|---|---|
| ⏻ Power   | `0x45` | ✓ verified |
| Mode      | `0x46` | standard |
| 🔇 Mute   | `0x47` | standard |
| ▶‖ Play   | `0x44` | standard |
| ◄◄ Prev   | `0x40` | ✓ verified |
| ►► Next   | `0x43` | standard |
| EQ        | `0x07` | ✓ verified |
| − (Vol−)  | `0x15` | standard |
| + (Vol+)  | `0x09` | standard |
| 0         | `0x16` | ✓ verified |
| ↻ Cycle   | `0x19` | ✓ verified |
| U/SD      | `0x0D` | standard |
| 1         | `0x0C` | standard |
| 2         | `0x18` | standard |
| 3         | `0x5E` | ✓ verified |
| 4         | `0x08` | ✓ verified |
| 5         | `0x1C` | ✓ verified |
| 6         | `0x5A` | standard |
| 7         | `0x42` | ✓ verified |
| 8         | `0x52` | standard |
| 9         | `0x4A` | ✓ verified |

## Notes for the (future) industrial IR driver

- **Ambient IR noise is normal here** (~25 `UNKNOWN` bursts / 3 s — sunlight / LED lighting). It
  is **automatically rejected**: NEC decoding validates the command against its bit-inverse, so
  only real presses pass. It only made *bulk capture* tedious; it does **not** affect operation.
- The receiver idles **HIGH** (`IR-idle=1`) — wiring is healthy.
- Driver plan (sentinel_module_spec SENT-004 sensor driver → SENT-LINK `E`vent): decode NEC, emit
  `{"s":"ir","cmd":<code>}`; the Pi bridge maps `cmd` → an **advisory** command request
  (explore / map / STOP …) that the Pi validates through its gates — **never** an ESTOP path (D-S7).
- `~/git/sentinel/irscan/` is the disposable capture scaffold that produced this table.
