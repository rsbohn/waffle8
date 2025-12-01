# Watchdog Timer (emulator device)

This document describes the Watchdog Timer device implemented in the PDP‑8 emulator.

## Overview
The watchdog is a wall-clock countdown timer that guest code can control via IOT instructions. When the countdown reaches zero, the device performs one of the configured actions (RESET, HALT, INTERRUPT, or TICK), and may be one-shot or periodic.

## Control Register (12 bits)
- **Bits 11..9: CMD** (3 bits)
  - `000` = disabled
  - `001` = RESET (one-shot)
  - `010` = RESET (periodic)
  - `011` = HALT (one-shot)
  - `100` = HALT (periodic)
  - `101` = INTERRUPT (one-shot)
  - `110` = INTERRUPT (periodic)
  - `111` = TICK (periodic, flag-only)
- **Bits 8..0: COUNT** (9 bits) — countdown value in deciseconds (0..511)

## Device Code
The watchdog timer uses **device code 055** (octal). IOT instructions are formed as `06000 | (055 << 3) | function`:
- Base: `6550` (octal)

## IOT Instructions

| Octal | Mnemonic | Function | Description |
|-------|----------|----------|-------------|
| 6550  | NOP      | 0        | No operation |
| 6551  | ISK      | 1        | Interrupt Skip if expired flag set |
| 6552  | WRITE    | 2        | Write control register from AC |
| 6553  | READ     | 3        | Read control register into AC |
| 6554  | RESTART  | 4        | Restart counter with current COUNT value |

### Usage Examples

**Set watchdog to HALT after 10 seconds (one-shot):**
```assembly
CLA CLL
TAD K_HALT_10SEC    / Load control word
IOT 6552            / Write to watchdog
...
K_HALT_10SEC, 03144 / CMD=3 (HALT one-shot), COUNT=100 (10 seconds)
```

**Periodic watchdog reset every 5 seconds:**
```assembly
CLA CLL
TAD K_RESET_5SEC    / Load control word
IOT 6552            / Write to watchdog
...
K_RESET_5SEC, 01062 / CMD=1 (RESET one-shot), COUNT=50 (5 seconds)
```

**Disable watchdog:**
```assembly
CLA             / AC = 0
IOT 6552        / Write zero to control register
```

**Restart/refresh watchdog timer:**
```assembly
IOT 6554        / Restart counter (useful in periodic loops)
```

## Implementation Details

### Timing Model
- Uses the host's monotonic clock (`clock_gettime(CLOCK_MONOTONIC)`)
- COUNT is interpreted as deciseconds (0.1 second units) and converted to nanoseconds internally
- Maximum timeout: 511 deciseconds (~51.1 seconds)
- The watchdog tick handler is invoked on each CPU cycle to check for expiry

### Command Behaviors

**RESET modes (CMD=1,2):**
- Sets PC to 0000 (reset vector)
- One-shot mode disables after firing; periodic mode reloads

**HALT modes (CMD=3,4):**
- Calls `pdp8_api_set_halt()` to stop CPU execution
- One-shot mode disables after firing; periodic mode reloads

**INTERRUPT modes (CMD=5,6):**
- Calls `pdp8_api_request_interrupt()` with device code 055
- Requires ION (interrupt enable) and ISR at 0010 to handle
- ISR should use ISK (6551) to poll watchdog and clear flag

**TICK mode (CMD=7):**
- Periodic flag-only mode for timing/polling
- Sets expired flag but takes no automatic action
- Use ISK (6551) to check if timer expired
- Flag latches until explicitly cleared by WRITE or RESTART

### Zero COUNT Handling
- COUNT=0 causes immediate expiry on next tick
- Useful for triggering immediate actions
- Still respects one-shot vs periodic behavior

## Configuration (pdp8.config)

The watchdog is enabled by default in the monitor and pdp8v virtual machine. No configuration file is required for basic operation. Programs control the watchdog via IOT instructions at runtime.

## Programming Examples

### Example 1: Simple Timeout Protection
```assembly
/ Set 10-second watchdog, get character, halt
*00100

START,
    CLA CLL
    TAD WDOG_CMD        / Load watchdog control word
    IOT 6552            / Set watchdog timer
    JMS GETCHR          / Get keyboard character (blocks)
    HLT                 / Halt with character in AC

WDOG_CMD, 03144         / HALT one-shot, 100 deciseconds (10 sec)

GETCHR, 0
GETCHR_WAIT,
    IOT 6031            / KSF: Skip if keyboard ready
    JMP GETCHR_WAIT
    IOT 6036            / KRB: Read character
    JMP I GETCHR
```

### Example 2: Periodic Watchdog Refresh
```assembly
/ Long-running loop that refreshes watchdog
START,
    CLA CLL
    TAD WDOG_CMD        / RESET periodic, 5 seconds
    IOT 6552            / Arm watchdog

LOOP,
    / ... do work ...
    IOT 6554            / RESTART: refresh watchdog timer
    JMP LOOP

WDOG_CMD, 01062         / RESET periodic, 50 deciseconds (5 sec)
```

### Example 3: Interrupt-Driven Watchdog
```assembly
/ Use watchdog to generate periodic interrupts
*0000
    JMP START

*0010                   / Interrupt service routine
ISR,
    DCA SAVE_AC         / Save AC
    IOT 6551            / ISK: Skip if watchdog expired
    JMP ISR_EXIT
    / Handle watchdog event
    IOT 6552            / Clear watchdog by rewriting control
ISR_EXIT,
    TAD SAVE_AC         / Restore AC
    ION                 / Re-enable interrupts
    JMP I 0006          / Return from interrupt

*0200
START,
    ION                 / Enable interrupts
    TAD WDOG_CMD
    IOT 6552            / Start watchdog in interrupt mode
    / ... main program ...
    HLT

WDOG_CMD, 06144         / INTERRUPT periodic, 100ds (10 sec)
SAVE_AC, 0
```

## Demo Programs

- **`slim/helper.pa`** - Minimal watchdog + keyboard input example
- **`demo/hello-wd.asm`** - Simple watchdog HALT demonstration
- **`demo/dull-boy.asm`** - Periodic watchdog refresh in a long-running program
- **`demo/wd-ticker.asm`** - Watchdog ticker mode for timing loops

## Best Practices

1. **Always set the watchdog before blocking operations** (keyboard input, device waits)
2. **Use appropriate timeouts** for your operation (10 seconds is typical for interactive input)
3. **Refresh periodic watchdogs frequently** in long-running loops
4. **Use explicit octal literals** for IOT instructions: `IOT 6552` not `IOT SYMBOL`
5. **Test watchdog behavior** in pdp8v where you can see the countdown timer
6. **Consider one-shot vs periodic** based on whether you need continuous protection



## Monitor Commands

### Disabling the Watchdog Interactively

From the monitor prompt, you can disable the watchdog by depositing and running a small sequence:

```
pdp8> dep 200 7300 6552 7402
pdp8> go 200
```

This deposits:
- `7300` - CLA (clear AC)
- `6552` - IOT 6552 (WRITE with AC=0, disabling watchdog)
- `7402` - HLT

### Checking Watchdog Status

Use the `show devices` command to see watchdog status in the monitor.

## API Reference (C)

```c
#include "watchdog.h"

// Create and attach watchdog
pdp8_watchdog_t *wd = pdp8_watchdog_create();
pdp8_watchdog_attach(cpu, wd);

// Query status
struct pdp8_watchdog_status status;
pdp8_watchdog_get_status(wd, &status);

// Status fields:
// - enabled: non-zero if counting
// - expired: non-zero if last expiry fired
// - cmd: raw command value (0..7)
// - configured_count: countdown in deciseconds (0..511)
// - remaining_ds: remaining time in deciseconds, -1 if not running

// Cleanup
pdp8_watchdog_destroy(wd);
```

## Testing

The watchdog implementation is tested in:
- `tests/test_emulator.c` - Core watchdog functionality tests
- `slim/helper.pa` - Integration test with keyboard input

## Troubleshooting

**Problem:** Watchdog fires immediately
- **Cause:** COUNT set to 0
- **Solution:** Use non-zero COUNT value (100 deciseconds = 10 seconds is typical)

**Problem:** Watchdog doesn't fire
- **Cause:** CMD is 0 (disabled) or WRITE IOT wasn't executed
- **Solution:** Verify AC contains correct control word before IOT 6552

**Problem:** Can't break infinite loop in monitor
- **Cause:** Not using `go` command which pumps keyboard
- **Solution:** Use `go` instead of `c` or `t` for programs that wait for input

**Problem:** Program halts unexpectedly
- **Cause:** Watchdog timeout expired
- **Solution:** Increase COUNT value or refresh watchdog more frequently with IOT 6554

## Developer Notes

- Source files: `src/emulator/watchdog.c`, `src/emulator/watchdog.h`
- Device code: 055 (octal) - IOT base 6550
- Uses POSIX monotonic clock for accurate timing
- Registered as tick callback in CPU core
- Fully integrated with monitor and pdp8v virtual machine
