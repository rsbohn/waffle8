/ 2025-11-27 – TC08 block write flow working

- Built a tiny `tools/rlink` utility to merge multiple SREC ROMs with a word-based start address; fixed the byte/word mismatch in the S9 record and regenerated `rom_combined` so the monitor starts at 03000.
- Improved `tools/moet` so bare references to colon-defined subroutines become `JMS label`; this fixed `DT1P`/`PUTS` being emitted as literals in `blin2`.
- Regenerated `sy/blin2.pa`, `rom3200`, `rom3000`, and `rom_combined` with the corrected expansions and start vector.
- Verified end-to-end in the monitor: ran BLING at 03000, then kicked DT1P at 03200. The TC08 write completed and block 1 on `magtape/tc08-unit1.tu56` shows the expected first word (`6552`) with the rest zeroed.
