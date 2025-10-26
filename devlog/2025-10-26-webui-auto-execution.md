# 2025-10-26 — Web UI auto-execution and layout refinements

## Highlights
- Added automatic execution mode running at 10 cycles/sec when not halted
- Enlarged console window to display ~5 lines of teleprinter output
- Introduced `pdp8_api_set_halt()` so the emulator starts in a halted state
- Cleaned up console input layout by removing redundant keyboard label

## Auto-execution implementation
- JavaScript timer fires every 100ms, checking halt status before executing one cycle
- Emulator now starts with HALT asserted via new `pdp8_api_set_halt()` API call
- Auto-run begins on page load and continues until HLT instruction or manual halt
- Provides a more natural PDP-8 front panel experience with continuous operation

## UI polish
- Increased `.card--stacked` min-height from 240px to 320px for better console visibility
- Set `.log-window` min-height to 7.5em (approximately 5 lines with 1.5 line-height)
- Removed "Keyboard input" label from console section for cleaner layout
- Input row now uses center alignment instead of end alignment

## C emulator changes
- Added `pdp8_api_set_halt()` function to `src/emulator/main.c` and `pdp8.h`
- Function signature mirrors `pdp8_api_clear_halt()` for API consistency
- Updated `tools/webdp8.py` to call `set_halt()` immediately after CPU creation
- Rebuilt shared library and copied to `factory/libpdp8.so`

## Testing notes
- Auto-execution stops cleanly when programs reach HLT
- Console output accumulates properly during automatic runs
- Manual Step/Run/Halt buttons remain functional alongside auto-execution
- Register display updates every 100ms during active execution
