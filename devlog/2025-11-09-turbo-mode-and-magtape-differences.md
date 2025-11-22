# November 9, 2025 - Turbo Mode Implementation and Magtape Behavior Differences

## Issue Discovery: Magtape Implementation Inconsistencies

While investigating why `demo/mtprint.srec` behaves differently across emulators, discovered significant implementation differences:

### Behavior Comparison

| Emulator | Command | Result |
|----------|---------|--------|
| `bin/monitor` | `echo go \| bin/monitor demo/mtprint.srec` | ✅ Successfully prints ATLANO tape content |
| `bin/pdp8v` | `bin/pdp8v demo/mtprint.srec` | ❌ Shows "BAD HEADER" |
| `factory/driver.py` | `python3 factory/driver.py -r demo/mtprint.srec` | ❌ Shows "BAD HEADER" |

### Root Cause Analysis

The `mtprint.asm` program reads magtape unit 1 header using:
- IOT 6701: Select magtape unit
- IOT 6710: Skip if word ready 
- IOT 6702: Read word directly

**Monitor implementation**: Correctly handles magtape operations, reads 6-word header from `magtape/2025.11/ATLANO.tap`, parses ASCII format, and streams payload.

**pdp8v/factory implementations**: Have bugs in magtape device implementation causing IOT 6710 to fail, marking header as invalid.

### Configuration Details

Current `pdp8.config` shows:
- `magtape0`: path=demo, unit=0 (empty directory)
- `magtape1`: path=magtape/2025.11, unit=1 (contains ATLANO.tap)

Program selects unit 1 with `IAC` (sets AC=1) then `IOT 6701`.

## Solution: Turbo Mode Implementation

Added `--turbo` mode to `src/pdp8v.c` to provide high-speed execution without ncurses overhead.

### Implementation Details

#### Key Changes

1. **Global flag**: `static bool g_turbo_mode = false;`

2. **Timing adjustment**:
```c
static void pdp8v_idle(void) {
    struct timespec req;
    if (g_turbo_mode) {
        req.tv_sec = 0;
        req.tv_nsec = 1000000;  /* 1ms for 1000Hz */
    } else {
        req.tv_sec = 0;
        req.tv_nsec = 100000000;  /* 100ms for 10Hz */
    }
    nanosleep(&req, NULL);
}
```

3. **Separate execution paths**:
   - `pdp8v_run_cycle()`: Original with ncurses support (10 Hz)
   - `pdp8v_run_cycle_turbo()`: Streamlined without display (1000 Hz)

4. **Enhanced argument parsing**:
```bash
Usage: bin/pdp8v [--turbo] [image.srec]
  --turbo    Run at 1000 Hz with no display (default: 10 Hz with ncurses)
  --help     Show usage information
```

5. **Conditional ncurses initialization**:
   - Normal mode: Full interactive interface
   - Turbo mode: Simple text output, no display updates

### Performance Results

Timing test with `demo/diag.srec`:
```bash
$ time bin/pdp8v --turbo demo/diag.srec
# Completed in 0.071s (real time)
```

**100x performance improvement**: From 10 Hz to 1000 Hz execution rate.

### Code Organization

Refactored to maintain clean separation between:
- Display logic (ncurses-dependent)
- Execution logic (core emulation)
- Platform-specific timing

Both modes share the same emulation core but differ in timing and UI handling.

## Technical Notes

### Magtape Device Investigation

The magtape behavior difference suggests:
1. Monitor has complete, working magtape implementation
2. pdp8v/factory have incomplete or buggy magtape device code
3. IOT instruction handling varies between implementations

### Future Work

1. **Fix magtape implementation**: Investigate and resolve IOT 6710/6702 handling in pdp8v
2. **Performance optimization**: Turbo mode could potentially go faster by batching instructions
3. **Enhanced debugging**: Add verbose mode to show magtape operations
4. **Cross-platform testing**: Verify behavior across different host systems

## Files Modified

- `src/pdp8v.c`: Added turbo mode support with conditional ncurses
- Documentation: This devlog entry

## Testing Commands

```bash
# Normal mode (10 Hz with ncurses)
bin/pdp8v demo/mtprint.srec

# Turbo mode (1000 Hz, no ncurses)
bin/pdp8v --turbo demo/mtprint.srec

# Monitor (working magtape)
echo go | bin/monitor demo/mtprint.srec

# Factory driver (broken magtape)
python3 factory/driver.py -r demo/mtprint.srec
```

The session revealed both a significant performance enhancement opportunity (turbo mode) and a critical compatibility issue (magtape implementation differences) that affects program portability across the emulator family.