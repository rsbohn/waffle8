# 2025-11-02 Visual Watchdog Monitor and Interrupt Testing

## Summary
- Added watchdog status display panel to `factory/visual.py`
- Added interrupt status display ([ION]/[IOFF]) to register line
- Added program listing toggle with '=' key
- Fixed interrupt test program (`demo/interrupt-test.asm`) for proper interrupt handling
- Watchdog monitor shows real-time status in visual front panel

## Visual Front Panel Enhancements

### Watchdog Status Panel
Added a dedicated watchdog monitor panel in the top-right corner of the visual front panel:

```
+--------------------------
| WATCHDOG
| Mode: INTERRUPT_PERIODIC
| Timeout: 20ds
| Remain: 18ds
| Expired: NO
+--------------------------
```

**Implementation details:**
- Defined `WatchdogStatus` ctypes structure matching C `struct pdp8_watchdog_status`
- Added `WD_CMD_NAMES` dictionary for human-readable mode names
- Configured `pdp8_watchdog_get_status` API for runtime status queries
- Panel displays mode, configured timeout, remaining time, and expiry flag
- Updates in real-time during execution

**Layout changes:**
- Watchdog panel positioned in top-right (28 characters wide)
- Main register info compacted on left side
- ASCII box drawing (no right border to avoid terminal width issues)
- Dynamic layout only shows panel when watchdog device attached

### Interrupt Status Display
Added interrupt enable/disable status to register display line:
- Shows `[ION]` when interrupts are enabled
- Shows `[IOFF]` when interrupts are disabled
- Uses `pdp8_api_is_interrupt_enabled` API
- Updates in real-time during program execution

Example display:
```
PC: 0204  AC: 0000  LINK: 0  HALT: no  [ION]
```

### Program Listing Toggle
Added '=' key to toggle between console output and program listing:
- **Console mode** (default): Shows KL8E console output
- **Listing mode**: Shows memory dump starting at current PC
- Format: `> 0204  1213` (marker, address, word value)
- Current PC marked with `>` indicator
- Auto-scrolls with PC during single-step debugging

Controls updated:
```
Controls: [space]=pause/resume [s]=step [=]=listing [q]=quit
```

## Interrupt Test Program Fixes

Fixed `demo/interrupt-test.asm` to properly demonstrate interrupt-driven I/O:

### Architecture Clarification
- **Interrupt vector at 0020** (not 0010) - custom design to avoid collision with LINK save area
- Context save layout:
  - 0006: Saved AC
  - 0007: Saved PC
  - 0010: Saved LINK
  - 0020: ISR entry point

### Addressing Issues Resolved
1. **ISR location**: Set to `* 0020` per custom interrupt vector design
2. **Reset vector**: `JMP I 0002` jumps indirect through address 0002 containing START (0200)
3. **Indirect addressing**: Added pointer variables for cross-page data access
   - `PTICKS` → `TICKS` (ISR data at 0215)
   - `PINBUF` → `INBUF` (ISR data at 0216)
4. **Entry point**: Address 0002 contains START for reset vector

### Assembly Compatibility
- Replaced `ISK` mnemonic with `IOT` (ISK not in assembler)
- Replaced `NOP` with `7000` (group 2 operate, no bits)
- Used `IAC` instead of `TAD =1` (literal syntax not supported)
- Direct `6001` for ION instruction

### Watchdog Configuration
- `WDMODE = 06024` configures INTERRUPT_PERIODIC mode:
  - CMD [11:9] = `110` = 6 = INTERRUPT_PERIODIC
  - COUNT [8:0] = `000010100` = 20 deciseconds = 2.0 seconds
- Test validates single interrupt (no restart in ISR - testing one-shot behavior)

## Testing

Assembled `demo/interrupt-test.asm` successfully:
- 33 words assembled
- Origins at 0000, 0002, 0006, 0020, 0200
- Proper ISR structure with device polling
- Reset vector points to START at 0200

## Files Modified
- `factory/visual.py`: Added watchdog monitor panel, interrupt status, and program listing toggle
- `demo/interrupt-test.asm`: Fixed for proper interrupt handling and assembly compatibility
- `demo/interrupt-test.srec`: Updated with latest assembly
- `demo/wd-test.asm`: Created simple watchdog test (HALT_PERIODIC mode)

## Notes
- Watchdog monitor provides visibility into timer state during debugging
- Interrupt status display shows ION/IOFF state for interrupt debugging
- Program listing mode allows following execution flow without console clutter
- Visual front panel supports single-step debugging of ISR execution
- Interrupt test demonstrates complete interrupt lifecycle with device polling
- All watchdog modes (DISABLE, RESET, HALT, INTERRUPT) × (ONE_SHOT, PERIODIC) supported
- Custom interrupt vector at 0020 (instead of standard 0010) due to context save requirements
