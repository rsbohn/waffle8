# 2025-10-14 – Monitor S-record Loader and Print Tape Fixes

## Overview
Extended the host monitor to import Motorola S-record images directly and tightened the execution controls so front-panel style runs behave predictably. Also taught the PAL-like assembler to accept symbol operands in data words, which let the paper tape demo pass string pointers without raw octal constants. The `print-tape` sample now prints a friendly header and the expected tape contents instead of the garbled ASCII that leaked from uncleared registers.

## Notable Changes
- Added a `read <file>` monitor command that parses S1/S2/S3 records, writes decoded 12-bit words into core, and applies the optional entry-point record to the PC (with helpful diagnostics for malformed records).
- Updated the `run` command to require an explicit start address and cycle count; the monitor loads the PC before delegating to the time-sliced execution loop so operator-driven runs no longer depend on residual state.
- Enhanced `tools/pdp8_asm.py` to recognise symbols in data statements, paving the way for pointer tables and other indirections without manual octal calculations.
- Reworked `print-tape.asm` to clear AC before pointer loads, introduce message pointer words, and zero AC after teleprinter output so the KL8E emits the expected “Read block …” banner followed by the tape block in octal.

## Validation
```shell
$ cc -std=c11 -Wall -Wextra -pedantic tools/monitor.c \
      src/emulator/main.c src/emulator/board.c \
      src/emulator/kl8e_console.c src/emulator/line_printer.c \
      src/emulator/paper_tape.c src/emulator/paper_tape_device.c \
      -o monitor
$ python3 tools/pdp8_asm.py print-tape.asm print-tape.srec
$ printf "read print-tape.srec\nrun 200 5000\nquit\n" | ./monitor
Read block 0001
5252 5252 5252 5252 5252 5252 5252 5252 
...
```

## Follow-up – Monitor UX and Fibonacci Sample
- Reformatted the monitor `mem` dump so it emits eight words per line with repeated address headings, matching PDP-8 service printouts.
- Renamed the `load` command to the more authentic `dep`, keeping the existing multi-word deposit behaviour and updating help text.
- Added a `fib.asm` sample that prints the first five Fibonacci numbers via the KL8E console, demonstrating the assembler + monitor workflow end-to-end.
- Synced `README.md` to highlight the Python assembler, S-record inspection tool, monitor commands, and the `python3 -m factory` entry point.

## Monitor Enhancements
- Added a `pdp8_api_clear_halt` helper so front ends can resume a halted CPU without trampling AC or link; the monitor now calls it before `run` or single-step loops so stale HALT latches no longer block execution.
- Introduced a shorthand `c [cycles]` command that continues from the current PC for the requested cycle count (default 1), relying on the same resume path as `run` and reporting the new PC/HALT state after each slice.
