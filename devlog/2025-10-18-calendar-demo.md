# 2025-10-18 – Calendar Console Demo

## Overview
Built a KL8E-driven “`% `” console demo (`demo/cal.asm`) that accepts a Gregorian year and month, validates them, and prints the corresponding calendar page. The program now sits alongside `demo/fib.asm` as another loadable PDP-8 example.

## Notable Changes
- Introduced a zero-page jump table so subroutines remain callable regardless of the assembled location; the actual code lives at `0200`.
- Implemented a decimal parser with whitespace skipping, echoing keyboard input while reporting malformed or out-of-range values and returning to the main prompt.
- Added leap-year aware date arithmetic anchored to 1957 (PDP-8 launch year) and table-driven month names/lengths, reusing the KL8E I/O helpers for aligned calendar output.
- Dropped in `demo/cal1970.asm`, a trimmed variant that prints the January 1970 calendar immediately and halts—handy for smoke-testing console output without interactive input.
- Taught `tools/pdp8_asm.py` to prefer a `START` label when emitting the S9 record so loaders that honour the entry point don’t get sent to the lowest data word.
- Updated both factory front-ends to respect the S-record start address: the Python runner now defaults to it, and the C driver installs the reset vector pointing at the same entry (unless the S-record omits one).

## Validation
```shell
$ python3 tools/pdp8_asm.py demo/cal.asm demo/cal.srec
$ py -m factory demo/cal.srec            # reset vector now lands on 0200
$ py factory/emulator/main.py demo/cal1970.srec --quiet
```
- Load `demo/cal.srec` into your PDP-8 monitor or simulator, point it at the KL8E console, and exercise a few inputs (e.g. `1984 7`) to confirm the layout and error handling.

## Follow-Ups
- Consider hoisting the shared KL8E I/O routines (prompting, decimal printing) into a common include once we have more console programs.
- Add regression coverage in `tests/` by driving the new demo under the emulator core when the harness grows KL8E stdin/stdout scripting support.
