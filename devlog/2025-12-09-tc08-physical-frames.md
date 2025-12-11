# 2025-12-09: TC08 Physical Frame Enforcement & Tape Defaults

## Summary

Two TC08 cleanups today:
1. Tightened the DECtape controller so it only reads/writes physical 129-word frames, matching how the `.tu56` images are actually stored.
2. Synced every executable, tool, and manual on the new default tape filenames (`media/tape0.tu56` for the read-only boot image and `media/tape1.tu56` for the writable unit).

`make -C tests` still builds (same unused-function warnings as before) after the emulator change.

## Physical Frames Only

- `src/emulator/tc08_device.c` now treats every block as two 129-word frames.
  - Rejects non-frame-sized images at load time, warning once per unit.
  - DTLB writes split the 256-word buffer into two frame payloads and zero the checksum slots before flushing.
  - Reads stitch consecutive frames back together for DMA.
- Removed the `os8_logical_layout` heuristic from `tc08_unit_t`; the controller no longer supports logical-block images directly.
- Updated `man/1/tc08.1` to document the physical-frame requirement.

## Default Tape Filenames

- All host entry points (`src/monitor.c`, `src/monitor_tc08.c`, `src/pdp8v.c`, `tools/webdp8.py`) fall back to `media/tape0.tu56` and `media/tape1.tu56` when the `TC08_IMAGEx` env vars are unset.
- Documentation touches:
  - `man/1/tc08.1`, `man/1/webdp8.1`, `docs/webdp8.md`, and `man/1/DECtape.1` now mention the new filenames in examples/env descriptions.
- Left the historical devlog references alone for provenance.

## Follow-ups

- Consider adding a small helper that can convert legacy logical-block `.tu56` images into physical frames so users dont have to run DECtape tooling by hand.
- webdp8 UI could expose a picker for alternate tape images now that the defaults live under `media/`.
