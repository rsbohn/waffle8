# 2025-11-03 Monitor “go” command and magtape printer tweaks

## Summary
- Added a `go` command to `tools/monitor`, letting the PDP-8 run without a fixed cycle budget until halt, an interrupt becomes pending, or the operator types `.`.
- Migrated `demo/mtprint.asm` output from the KL8E console to the line printer channel so header/payload dumps land in the printer transcript.
- Hooked `make -C tests pdp8_tests` followed by `./tests/pdp8_tests` into the workflow to ensure changes pass the existing emulator suite.

## Monitor `go`
- The command accepts an optional start address (`go 0200`) and otherwise continues from the current PC.
- Execution loops in `go_run_until_event`, which checks interrupts, halt state, and queued keyboard input between `MONITOR_RUN_SLICE` bursts.
- Keyboard handling treats `.` as a synchronous break request while still feeding other characters to the console device.
- On exit the command reports stop reason, cycle count, and register snapshot (`PC`, `AC`, `LINK`, `HALT`, `ION`, pending interrupt count).

## Magtape Printer Utility
- Swapped `mtprint`’s `PUTCHR` routine to poll `IOT 6601` / `IOT 6606`, matching the rest of the line-printer pipeline.
- Documentation banner now calls out the printer dependency explicitly.

## Verification
- `python3 tools/pdp8_asm.py demo/mtprint.asm demo/mtprint.srec`
- `make -C tests pdp8_tests` && `cd tests && ./pdp8_tests`
