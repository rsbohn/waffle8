# 2025-10-31 Visual Front Panel

## Summary
- Factory runner now keeps executing after stdin EOF so scripted launches do not stall at zero cycles.
- Added `factory/visual.py`, a curses front panel that displays PDP-8 registers, KL8E output, and supports pause/run/step control with a paused-on-start default.
- Tweaked the panel's single-step key to issue one `pdp8_api_step`, giving accurate PC updates during manual debugging.

## Notes
- The panel reuses the existing factory helper logic for device wiring, keeping peripheral setup consistent with the non-interactive driver.
- KL8E output is rendered into a scrollback pane while keystrokes are queued back into the console device, mimicking the teleprinter.
- Non-interactive shells are refused up front so `curses.wrapper` does not explode when someone pipes the script.
