# 2025-11-02 Papertape→Magtape Tooling and Monitor Docs

## Summary
- Added `demo/mtcp.asm`, a papertape-to-magtape copier that prints each word it copies.
- Hardened magtape manifest loading to tolerate truncated `.tap` placeholders.
- Updated `pdp8.config` defaults so magtape unit 0 is read-only demo media and unit 1 is the writable spool.
- Documented both monitor front-ends with new man pages (`monitor(1)` and `monitor_driver(1)`).

## MTCP Utility
- New program walks paper tape blocks sequentially, counts each block, emits a PDP-8 magtape header (length + SIXBIT label/format), and streams the data to unit 1.
- Every papertape word is printed to the KL8E as a four-digit octal value; blocks end with CR/LF for readability.
- Program honours the switch register to choose a starting block and halts when the paper tape device reports no more data.
- Teleprinter output routines live on page 0400; the main loop calls them via a pointer table in zero page.

## Magtape Runtime Improvements
- `pdp8_magtape_device` now treats truncated records as partial instead of aborting configuration, allowing empty `.tap` placeholders (e.g., `000.tap`) to coexist with writable directories.
- Default configuration (`pdp8.config`) mirrors the docs: `magtape0` is the demo reel (read-only) and `magtape1` writes into `magtape/`.

## Monitor Documentation
- Wrote `man/1/monitor.1` covering the interactive command set, numeric syntax, and typical workflows.
- Wrote `man/1/monitor_driver.1` describing the batching helper, command-file format, prompt detection, and usage examples.
- Noted that `t N` takes an explicit cycle count (e.g., `t 5`) for precise traces.

## Testing & Notes
- Rebuilt tools with `make -C tests`.
- Verified `monitor_driver.py --image demo/mtcp.srec 'magtape 1 new' 'run 0200 #200000' quit` emits the expected octal trace and creates new magtape records inspectable via `tools/magtape_tool.py`.
- `mtcp` relies on the paper tape driver returning individual blocks; run `magtape new 1` beforehand to start a fresh output file.
