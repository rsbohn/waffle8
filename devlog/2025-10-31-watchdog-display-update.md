# 2025-10-31: Watchdog Status Display Updated for Interrupt Modes

## Overview
Updated the monitor's `show watchdog` command to correctly display the new interrupt mode command codes (5 and 6) that were added during Phase 1 interrupt support implementation.

**Status: ✅ COMPLETE** | Files: 1 | Changes: 4 lines

## Work Summary

### Issue Identified
The `show watchdog` command in the monitor was missing display support for the two new interrupt-mode commands introduced in Phase 1:
- Command 5: `PDP8_WD_CMD_INTERRUPT_ONE_SHOT`
- Command 6: `PDP8_WD_CMD_INTERRUPT_PERIODIC`

When the watchdog was configured in interrupt mode, the monitor would fall through to the default "reserved" label instead of displaying the correct mode name.

### Fix Applied
**File: `tools/monitor.c`** (function `show_watchdog()`, lines 414-434)

Added two new switch cases to the command descriptor switch statement:

```c
case 5:
    cmd_desc = "interrupt (one-shot)";
    break;
case 6:
    cmd_desc = "interrupt (periodic)";
    break;
```

This ensures the monitor now properly reports:
- `interrupt (one-shot)` when cmd=5
- `interrupt (periodic)` when cmd=6

Along with the existing modes:
- `disabled` (0)
- `reset (one-shot)` (1)
- `reset (periodic)` (2)
- `halt (one-shot)` (3)
- `halt (periodic)` (4)

### Display Output Example

```
show watchdog
Watchdog (runtime):
  enabled          : yes
  cmd              : interrupt (periodic) (6)
  configured count : 50 deciseconds
  time remaining   : 23 deciseconds
  expired          : no
```

## Technical Details

The watchdog status structure (`struct pdp8_watchdog_status`) already tracked the correct command values; the monitor's display layer simply needed updating to interpret them. The core emulator logic (watchdog.c and watchdog.h) already implements the interrupt commands correctly.

## Related Files
- `tools/monitor.c`: Updated `show_watchdog()` function
- `src/emulator/watchdog.h`: Defines interrupt command constants (lines 23-24)
- `src/emulator/watchdog.c`: Implements interrupt dispatch via `pdp8_api_request_interrupt()`

## Testing
Verified that rebuilding the monitor and running `show watchdog` now displays interrupt modes correctly when the watchdog is in interrupt mode.
