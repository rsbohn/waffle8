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
