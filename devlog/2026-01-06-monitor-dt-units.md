# 2026-01-06: Monitor DT Unit Commands

## Summary

Extended the monitor’s DECtape workflow so the TC08 controller can be driven
directly from the prompt with per-unit commands. The `tc08` command was
renamed to `dt`, and new `dt0`/`dt1` subcommands support attaching/detaching
tape images and copying blocks to/from memory.

## New Monitor Commands

- `dt` now reports TC08 status, including attached/detached state per unit.
- `dt0` and `dt1` manage individual units:
  - `att/attach <file> [new]` attaches a tape image, creating it when `new` is
    supplied.
  - `det/detach` detaches the tape from the unit.
  - `read <block> <address>` loads a block into memory.
  - `write <block> <address>` writes a block from memory and flushes to disk.
- `show` help lists `dt` instead of the deprecated `tc08` topic.

## Emulator Wiring

- The monitor now holds a pointer to the attached TC08 device via a new
  `pdp8_api_get_tc08_device()` accessor.
- Added TC08 helper APIs to attach/detach tape files and perform block reads or
  writes without going through PDP-8 IOT sequences.

## Notes

- I didn’t run `make -C tests` for this change set.
