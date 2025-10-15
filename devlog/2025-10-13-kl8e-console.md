# 2025-10-13 – KL8E Console Integration

## Overview
Implemented a host-side KL8E console so RTS-8 and other terminal-driven software can run against the emulator. The keyboard (device code 03) and teleprinter (device code 04) handlers now exist as reusable helpers inside `src/emulator`, mirroring DEC’s KSF/KCC/KRB and TSF/TCF/TLS micro-ops. This replaces ad hoc console stubs in the front ends and keeps the device logic centralized.

## Notable Changes
- Added `pdp8_kl8e_console_{create,attach,...}` along with opcode macros for unit tests and tooling (`src/emulator/kl8e_console.[ch]`). Keyboard input is buffered so the PDP-8 sees one character at a time; output is logged for retrieval by front ends.
- Updated `tools/monitor` to attach the KL8E, pump stdin without blocking the REPL, and translate LF→CR before queueing to match the KL8E’s expectations. Execution now runs in small time slices to interleave console polling.
- Extended the Python factory runner to attach the console via ctypes, use `select()` on stdin for interactive sessions, and flush buffered output after each run block.
- Added a regression in `tests/test_emulator.c` that asserts KSF/KRB/TLS semantics and linked the new device into the test binary via `tests/Makefile`.
- Authored a `hello-kl8e.asm` line-echo sample (buffered KL8E KSF/KRB/TLS loop) and emitted a `hello-kl8e.srec` image so the console path can be demoed without RTS-8.

## Validation
```shell
$ make -C tests
$ ./tests/pdp8_tests
All 7 tests passed.
```

Factory smoke test with manual console interaction also succeeded; RTS-8 boots to the monitor prompt when provided with the appropriate image.
