# 2025-10-25 — Single-Month Calendar Polishing

## What changed today
- Rebased `cal3.asm` so the entry routine lives at `0200`, with helpers pushed to higher pages (`0400`, `0600`, `01000`, `02000+`) to open up page 1 for the start sequence.
- Added a `FIRSTDAY` trampoline plus `FIRSTDAY_IMPL` to compute the weekday pointer from the template offset; zero-page now tracks `FIRST_DAY`/`FIRST_DAY_PTR` alongside the existing workspace.
- Migrated the month input table to page `02000` and adjusted the Python harness to poke `MONTH_IN` via the `/mem` endpoint (`demo/scripts/cal3demo.py` now writes to `02001`).
- Introduced `tools/webdp_trace.py`, a neat table-view wrapper over webdp8’s `/trace` endpoint with filters for PC/instruction/step.
- Dropped a quick mention of the trace tool into the README host-tool list.
- Spun a web UI skeleton (`factory/ui/…`) and a page allocator visualiser (`tools/allocated.py`) to keep tabs on the new layout.

## Test notes
- `python3 tools/pdp8_asm.py demo/cal3.asm demo/cal3.srec`
- `python3 tools/allocated.py demo/cal3.asm`
- `/opt/boston/bin/python demo/scripts/cal3demo.py --start 0200 --cycles 4096 --max-iter 10`
- `python3 tools/webdp_trace.py $(cat trace-args)` for selective trace peeks

## Current status / follow-ups
- Latest cal3 run prints `1962 October`; still need to confirm the weekday print path fires (trace suggests `JMS FIRSTDAY` isn’t getting invoked yet).
- `FIRSTDAY_IMPL` stores a pointer, but zero-page values stay zero after the run—need to double-check indirect jumps around the new vector.
- `webdp_trace.py` works but still needs better failure reporting when the filter returns no rows (currently just “(no steps to display)”).
- UI skeleton is static HTML/CSS; next step is to wire API calls (ROM upload, register refresh) once the backend endpoints settle.
