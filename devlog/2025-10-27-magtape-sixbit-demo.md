# 2025-10-27 — Magtape header fixes and SIXBIT demo reel

## Summary
- Patched `demo/mt.asm` so the six-word tape header loads correctly and the selected unit prints without corrupting the `CHARMAP`.
- Produced a companion SIXBIT sample reel (`magtape/demo-hello-sixbit.tap`) and documented both demo tapes in the usage guide.

## Details
- Adjusted the header pointer sequencing (`HDR_PTR = HEADER-1`) so the auto-increment store captures all six words. This also removed the extra digit buffer write that had been clobbering buffer slots.
- Stopped zeroing `CHAR_TEMP` before `PUTCHR`, letting the unit digit flow straight from `AC`.
- Relocated the `CHARMAP` table to page `0740` to keep it clear of the decoded label/format buffers during subsequent iterations.
- Generated a new SIXBIT magtape record (label `SIXBIT`, format `SIXBIT`, payload “SIXBIT DEMO 01”) to sit alongside the existing ASCII reel. Updated `.gitignore` so the tape can live in version control.
- Refreshed `docs/magtape-usage.md` with a demo tape table calling out the ASCII and SIXBIT samples.

## Verification
- `python3 tools/monitor_driver.py --image demo/mt.srec "switch load 0200" "c 4000"` — header inspector now reports `UNIT: 0`, `LABEL: ASCII0`, `FORMAT: ASCII ` with the ASCII reel, and `LABEL/FORMAT: SIXBIT` when the SIXBIT tape is attached.
- `python3 tools/magtape_tool.py inspect magtape/demo-hello-sixbit.tap --head --decode` — confirms the header fields and decodes the payload into “SIXBIT DEMO 01”.
