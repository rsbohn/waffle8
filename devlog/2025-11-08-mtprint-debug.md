## 2025-11-08 mtprint binary debug

- Added BINARY format support to `demo/mtprint.asm`: new tag/descriptor constants, format dispatcher branch, and `BINARY_PAYLOAD` routine that streams payload words as four-digit octal, eight words per line. Introduced `PRINT_WORD_OCT` helper plus extra zero-page scratch (`BIN_WORD_COUNT`, `OCT_WORK`, `OCT_DIGIT`).
- Precomputed all negative constants (`NEG6`, `NEG1000`, etc.) in zero page so the runtime no longer depends on assembler unary minus.
- Discovered that the historical origins (`*0500`, `*0520`, `*0545`) caused `STORE_PAIR` and `SHIFT_RIGHT6` to overwrite the expanded dispatcher (instructions at 0525+). Relocated those helpers to page `01700` to keep the main routine contiguous.
- The SIXBIT/PUTCHR helpers initially sat at `*0120`/`*0640`, which stomped the zero-page pointer table and the charmap data. Moved the whole block to page `01100` (with `PUTCHR` following sequentially) so no code lives in zero page or the charmap’s address range.
- Rebuilt `demo/mtprint.srec` and pushed it into the running web emulator via `curl -F file=@demo/mtprint.srec http://192.168.0.250:5000/loader`. Verified via `/mem` API calls:
  - Zero page shows the new constants and pointer table (`ASCII_PAYLOAD_PTR` now at 0115, etc.).
  - Dispatcher now contains `0525:5331` (JMP TRY_BINARY) followed by `0526:4515` (JMS I ASCII_PAYLOAD_PTR), eliminating the earlier `JMS I 0020` → `0001` fallthrough.
- Confirmed that the remote emulator is now executing the updated mtprint binary, matching the current source and listing. Final sanity tests:
  - ASCII reel (`magtape/2025.11/ATLANO.tap`): `dep 7 1` + `go 200` prints “MAG1 ATLANO [ASCII]” followed by the narrative payload and halts cleanly.
  - BINARY reel (`magtape/test/0000.tap` from the write-many harness): `dep 7 1` on the test reel shows the four-digit octal dump per record.
  - Unit 0 remains on the demo fixture reel (fixture.srec) and stays ready-only. `show magtape` verifies both units after each load.
