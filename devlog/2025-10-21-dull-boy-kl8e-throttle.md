# October 21, 2025 - Dull Boy demo and KL8E pacing tweaks

## Goals
- Produce an "All work and no play" demonstration program that is self-contained on page 1.
- Slow the demo output so it feels like a real teleprinter instead of a blur.
- Let the factory driver auto-run demos without dropping into the REPL.

## Changes
- `demo/dull-boy.asm`: Added PUTS/PUTCH routines, a two-level `SLOWLY` busy-loop, and the full sentence payload. Program assembles to 0200 and loops forever.
- `factory/driver.py`: Introduced 110 baud pacing by draining the KL8E output queue each cycle and sleeping ~90 ms per character. Added an `-r/--run` flag that skips the interactive `go` prompt. Imported `time` and exposed tuning constants.
- `demo/dull-boy.srec`: Regenerated from the updated assembly.

## Validation
- `python3 tools/pdp8_asm.py demo/dull-boy.asm demo/dull-boy.srec`
- `python3 -m py_compile factory/driver.py`
- `python3 factory/driver.py -r demo/dull-boy.srec`

## Notes
- The emulator already buffered console output, so the pacing code just counts popped bytes; adjust `KL8E_BAUD` or the SLOWLY loop constants if future demos need different tempos.
- `-r` is handy when scripting regression runs that shouldn’t hang on the prompt.
