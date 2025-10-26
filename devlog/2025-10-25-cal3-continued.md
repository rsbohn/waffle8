# 2025-10-25 — Cal3 Calendar Rendering Plan

We now have `cal3.asm` printing the year, month name, first weekday, and month length. The next milestone is to emit a full single-month calendar that mirrors the legacy output. This is the plan for implementing the remaining routines.

## 1. Output layout decisions
- Freeze the textual format (header + weekday masthead + 5/6 week grid). We will target the existing line-printer width (132 columns) but keep the layout to 20 chars per week row (7 columns + spacing).
- Decide whether blank weeks are represented by all spaces or by zero values. Document this in a short README note for future reference.

## 2. Data preparation in `cal3.asm`
- Extend zero-page workspace with:
  - `CUR_WEEK_PTR` (points into the template data returned by `CALC`)
  - `PRINT_COL` (tracks column position for space insertion)
  - `WEEK_REMAIN` (countdown to decide when to print CR/LF)
- Update `CALC` to leave the template pointer and month length ready for day-grid expansion; confirm the template arrays already store 7-day offsets.

## 3. New routines
- `PRINT_HEADER`: print week-day masthead (`DAY_HDR`) and ensure CR/LF appended.
- `PRINT_WEEK`: consumes the next 7 entries from `TEMPLATE_PTR`, converts them to absolute day numbers (1–31 or blank), and maps them into `PRT2D`. Handles leading spaces for blank cells (print two spaces instead of digits when value is 0).
- `PRINT_CALENDAR`: orchestrates the loop:
  1. Initialise `CUR_WEEK_PTR` and `DAY_REMAIN` (month length).
  2. For each week (up to 6):
     - Call `PRINT_WEEK`.
     - Break when `DAY_REMAIN <= 0`.
  3. Ensure a trailing CR/LF.
- Optional `PRINT_BLANK_WEEK` to skip logic when no days remain.

## 4. Integration into the main flow
- After printing “Year Month Weekday MonthLength”, call `PRINT_HEADER` then `PRINT_CALENDAR`.
- Adjust the existing CR/LF so they happen inside the new routines to avoid double spacing.
- Confirm the restart path still works (the `HLT`/`JMP START` pair stays at the tail).

## 5. Testing strategy
- Extend `demo/scripts/cal3demo.py` with a `--raw` option to capture multiple lines without deduping, so we can verify the full calendar body.
- Create small fixture months (e.g., February 1968 leap year, February 1969 non-leap) and check:
  - Weekday alignment.
  - Leading/trailing blanks.
  - Correct number of weeks (months that span 6 weeks should render an extra row).
- Use `tools/webdp_trace.py --pc` helpers to sanity-check the new loops.
- Your only means of communication with the emulator is via tools/webdp_trace.py.

## 6. Documentation updates
- Update the README host tools section with an example of capturing the full calendar output.
- Add a short note to the devlog summarising results once implemented.

## 7. Stretch goal
- Add a flag in `cal3.asm` to toggle long vs. compact calendar format (e.g., printing only weekdays). Not required for first pass, but leave room in zero-page for a `FORMAT_FLAGS` word.

## Progress notes (2025-10-25)
- Implemented `PRINT_HEADER`, `PRINT_WEEK`, and `PRINT_CALENDAR` to emit a six-row grid using the month length and weekday offset; verified layout against February 1968/1969 and June 2024 via the native emulator harness.
- Added the `--raw` flag to `demo/scripts/cal3demo.py` and documented the capture workflow in `README.md` so multi-line printer output can be inspected without trimming.
- Reassembled `demo/cal3.srec` from the updated source to keep the demo image in sync.
