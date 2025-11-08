## 2025-11-09 diag SIXBIT sanity

- Re-ran the tiny `demo/diag.asm` splitter, noticed the teleprinter printed “BC” even though the packed word at `TAG` is `00403`. Walked through the PDP-8 trace: the program copies that constant into `WORD`, runs `SIX_SPLIT`, then streams the two decoded bytes through `PUTS`.
- Audited the routine: `SIX_SPLIT` now seeds auto-increment register `0010` with the literal buffer address, shifts the high character left six places (`LEFTY`) before masking, and writes a null terminator. Verified pointer usage by reserving an explicit buffer page at `*5000`.
- Extended the SIXBIT→ASCII table at `*6077` from four entries to the entire alphabet so any code 01–32 now decodes correctly. (`TABLE` points at `TABLE6A`, and `PUTS` adds the SIXBIT code to that base before dereferencing.)
- Used `tools/allocated.py demo/diag.asm` to confirm only pages 024 (buffer) and 030 (code + table) are occupied—no overlap with other segments.
- Loaded the rebuilt `demo/diag.srec` via the monitor; `factory.driver.load_srec` reports the start word `06000` and `TAG @ 06006 = 00403`, matching source. Running `go` prints `DC` and halts after 57 cycles, which is correct for that SIXBIT pair (`4 → D`, `3 → C`). Running with `TAG = 00101` prints “ A”, demonstrating the left shift is behaving as expected.
