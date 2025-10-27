2025-10-26: papertape

Summary

Added a command-line override to the monitor tool to allow specifying a paper-tape image at startup.

What I changed

- `tools/monitor.c`
  - Added a new field to `struct monitor_runtime`: `paper_tape_image_override`.
  - Initialized the field in `monitor_runtime_init`.
  - Extended `main` to accept `--papertape <path>` and preserve the existing optional positional SREC image parameter.
  - Modified paper-tape device initialization to prefer the command-line override over the value from `pdp8.config`.

Verification

- Built the project with `make`. The build succeeded.
- Ran `monitor --papertape /home/rsbohn/emulation/waffle8/tapes/8asm-man.tape` and inspected devices with `show devices`.
  - Monitor printed the papertape image path in the device listing, indicating the override was used.
  - The device loader reported: "Warning: unable to load paper tape image '<path>'" — this comes from the device loader (`pdp8_paper_tape_device_load`) and likely indicates a format/path issue rather than the CLI plumbing.

Notes / next steps

- If the papertape image fails to load, inspect `src/emulator/paper_tape_device.c` or `src/emulator/paper_tape.c` to determine the accepted formats and error reporting behavior.
- Consider improving error reporting when `pdp8_paper_tape_device_load` fails (surface errno or loader message), or accept more tape formats.
- Optionally add a `--verbose` option to the monitor for more detailed startup diagnostics.

Command used to verify

```
printf 'show devices\nquit\n' | ./monitor --papertape /home/rsbohn/emulation/waffle8/tapes/8asm-man.tape
```

Finished.

Additional follow-up (loader, tests, visual tools)

- `src/emulator/paper_tape.c` and the paper-tape loader:
  - Discovered the `tapes/8asm-man.tape` file used ASCII-octal tokens rather than raw bit-strings.
  - Added an ASCII-octal fallback parser and a detect-and-parse step so payload lines are parsed as either bit-strings or ASCII-octal tokens.
  - Relaxed per-line limits for ASCII-octal tokens and improved diagnostics so the loader prints a concise summary on success, for example:
    - "paper_tape_load: Loaded 104 blocks from tapes/8asm-man.tape parser=ascii-octal label=MA"

- Tests and factory bindings:
  - Updated `factory/test_papertape.py` to bind a `pdp8_api_get_pc` accessor and to validate SKIP semantics by checking the CPU PC, which avoids flaky timing-based checks.
  - Exposed a small `PaperTapeStatus` structure for test introspection of loaded blocks and ready state.

- Visual tape reader (`demo/scripts/visual_tape_reader.py`):
  - Added an interactive curses-based viewer (and `--dump` mode) useful for inspecting tape blocks, parsers, and teleprinter output.
  - Implemented teleprinter append/undo, autoplay (space), and an AUTOPLAY indicator in the header.
  - Added a READY indicator to the header and head-info (READY=1 when the current read head points at an available word).
  - Fixed startup KeyError and a head-variable ordering bug so the curses viewer starts reliably.

Commands used to exercise the new pieces

```
./monitor --papertape tapes/8asm-man.tape
python3 demo/scripts/visual_tape_reader.py --dump tapes/8asm-man.tape
python3 demo/scripts/visual_tape_reader.py --curses tapes/8asm-man.tape
```

Status

- The monitor CLI accepts `--papertape` and successfully loads `tapes/8asm-man.tape` now that the loader accepts ASCII-octal format.
- The visual reader starts and shows READY/autoplay/teleprinter behavior; a few UI polish items (help legend, jump/search) remain as follow-ups.
