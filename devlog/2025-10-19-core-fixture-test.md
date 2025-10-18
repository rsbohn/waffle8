# 2025-10-19 – Core Fixture Regression

## Overview
Automated the demo “core + fixture” scenario so the PDP-8 board dispatch logic and line-printer path are exercised under the C harness.

## Notable Changes
- Added a minimal S-record loader to `tests/test_emulator.c` so regression tests can ingest Motorola images without shelling out to tooling.
- Registered a new `core fixture` check that boots `demo/core.srec`, runs the BIOS initialiser at `7030`, loads `demo/fixture.srec`, and asserts on the call counter, accumulator, and emitted output.
- Relaxed the line-printer smoke test to ignore ANSI colour escapes while still verifying that the printable character stream is intact.

## Validation
```shell
$ make -C tests
$ ./tests/pdp8_tests
```

## Follow-Ups
- Teach the harness to decode ANSI control sequences directly so future printer devices can opt-in without bespoke stripping.
- Consider promoting the shared S-record parsing into a reusable helper if additional tests start consuming ROM images.
