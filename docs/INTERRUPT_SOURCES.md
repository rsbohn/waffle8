# Interrupt Sources - Implementation Status

**Date**: 2025-10-30  
**Status**: Audit of what can raise interrupt flags in the emulator

## Executive Summary

**Currently**: Only **watchdog** can raise interrupts

**Potentially**: All 5 standard peripherals could raise interrupts (infrastructure ready)

## Current Interrupt Sources

### ✅ Watchdog Device (055) - ACTIVE

**Status**: Actively raises interrupts when configured

**Implementation**:
- File: `src/emulator/watchdog.c` (line 54)
- Function: `watchdog_fire()`
- Call: `pdp8_api_request_interrupt(cpu, PDP8_WATCHDOG_DEVICE_CODE)`

**Trigger Condition**:
```c
if (wd->cmd == WD_CMD_INTERRUPT_ONE_SHOT || wd->cmd == WD_CMD_INTERRUPT_PERIODIC) {
    pdp8_api_request_interrupt(cpu, 055);
}
```

**Configuration** (pdp8.config):
```
device watchdog {
  mode = halt           # Can be "halt", "reset", or "interrupt"
  periodic = false      # If true, periodic mode (6) instead of one-shot (5)
  default_count = 5     # Timer value in deciseconds
}
```

**How to Use**:
1. Set `watchdog.mode = "interrupt"` in config
2. Optionally set `watchdog.periodic = true` for repeated interrupts
3. Program sets `watchdog.enabled = true`
4. Program enables interrupts with ION
5. When timer expires, CPU dispatches to ISR at 0x0010

**Test Program Pattern**:
```asm
        ION             / Enable interrupts
        IOT 6551 6      / Start watchdog timer (device 55, operation 6)
LOOP,   JMP LOOP        / Wait for interrupt
        
        * 0010          / ISR entry point
INTR,   ISK 6551        / Check if watchdog expired
        JMP DONE        / Watchdog not ready
        IOT 6551 5      / Clear watchdog flag
        ION             / Re-enable interrupts
DONE,   JMP I 0x0007    / Return to interrupted program
```

## Devices NOT Currently Raising Interrupts

### ❌ KL8E Keyboard (003) - Skip-Only

**Current State**:
- Has `keyboard_flag` (set when input available)
- Provides ISK 6031 (skip if input ready)
- Does NOT call `pdp8_api_request_interrupt()`

**File**: `src/emulator/kl8e_console.c` (lines 93-95)

```c
void kl8e_console_push_input_byte(pdp8_kl8e_console_t *console, uint8_t byte) {
    console->keyboard_buffer = byte;
    console->keyboard_flag = true;  // Flag set, but no interrupt call
}
```

**To Enable Interrupts**:
1. Add interrupt mode flag to console config
2. When keyboard_flag is set AND interrupt mode enabled → call `pdp8_api_request_interrupt(cpu, 003)`
3. Update factory/driver.py to parse `kl8e_console.keyboard_interrupt = true`

**Phase 2 Task**: Add interrupt support to KL8E keyboard

### ❌ KL8E Teleprinter (004) - Skip-Only

**Current State**:
- Has `teleprinter_flag` (set when ready to print)
- Provides ISK 6041 (skip if ready)
- Does NOT call `pdp8_api_request_interrupt()`

**File**: `src/emulator/kl8e_console.c` (lines 176, 190)

```c
console->teleprinter_flag = true;  // Flag set, but no interrupt call
```

**To Enable Interrupts**: Same pattern as keyboard

**Phase 2 Task**: Add interrupt support to KL8E teleprinter

### ❌ Line Printer (060) - Skip-Only

**Current State**:
- Has ready flag (not printing)
- Provides ISK 6601 (skip if ready)
- Does NOT call `pdp8_api_request_interrupt()`

**File**: `src/emulator/line_printer.c` (line 87)

```c
if (printer->flags & PDP8_LINE_PRINTER_FLAG_READY) {
    pdp8_api_request_skip(cpu);  // Skip, but no interrupt
}
```

**Phase 2 Task**: Add interrupt support to line printer

### ❌ Paper Tape Reader (067) - Skip-Only

**Current State**:
- Has ready flag (data available)
- Provides ISK 6671 (skip if ready)
- Does NOT call `pdp8_api_request_interrupt()`

**File**: `src/emulator/paper_tape_device.c` (lines 34-36)

```c
if (microcode & PDP8_PAPER_TAPE_BIT_SKIP) {
    pdp8_api_request_skip(cpu);  // Skip, but no interrupt
}
```

**Phase 2 Task**: Add interrupt support to paper tape reader

### ❌ Magtape Device (070) - Skip-Only

**Current State**:
- Has multiple flags (done, error, etc.)
- Provides ISK variants for error checking
- Does NOT call `pdp8_api_request_interrupt()`

**File**: `src/emulator/magtape_device.c` (lines 802-804)

```c
if (microcode & PDP8_MAGTAPE_BIT_SKIP) {
    pdp8_api_request_skip(cpu);  // Skip, but no interrupt
}
```

**Phase 2 Task**: Add interrupt support to magtape

## Architecture Comparison

### Current (Polled I/O)
```
Program Loop:
  ISK 6031        / Check keyboard
  JMP NEXT        / Not ready, continue
  JMS HANDLE_KBD  / Handle input

NEXT: ISK 6041    / Check printer
  JMP LOOP        / Not ready, loop
  JMS HANDLE_PRN  / Handle printer
  JMP LOOP
```

**Characteristics**:
- ✅ Deterministic (program controls polling order)
- ✅ Simple (no interrupt service routines)
- ❌ Busy-waiting (inefficient)
- ❌ Latency depends on polling frequency

### Proposed (Interrupt-Driven I/O)
```
Program Main Loop:
  ION             / Enable interrupts
  ... do work ...

ISR @ 0x0010:     / Triggered by device flags
  ISK 6031        / Poll keyboard
  JMP NEXT
  JMS HANDLE_KBD
  
NEXT: ISK 6041    / Poll printer
  JMP DONE
  JMS HANDLE_PRN
  
DONE: ION         / Re-enable interrupts
  JMP I 0x0007    / Return to interrupted work
```

**Characteristics**:
- ✅ Event-driven (devices notify CPU)
- ✅ Efficient (no busy-waiting)
- ✅ Low latency (ISR runs immediately)
- ❌ More complex (ISR required)

## Phase 2 Implementation Plan

### Step 1: Add Interrupt Mode Configuration
Add to each device in pdp8.config:
```
device kl8e_console {
  keyboard_interrupt = false    # New: enable keyboard interrupts
  teleprinter_interrupt = false # New: enable teleprinter interrupts
}

device line_printer {
  interrupt_mode = false        # New: enable printer interrupts
}

device paper_tape {
  interrupt_mode = false        # New: enable reader interrupts
}

device magtape0 {
  interrupt_on_done = false     # New: interrupt when operation done
  interrupt_on_error = false    # New: interrupt on error
}
```

### Step 2: Update Device Handlers
Example for keyboard (src/emulator/kl8e_console.c):
```c
// In kl8e_console_push_input_byte():
console->keyboard_flag = true;

// NEW CODE:
if (console->interrupt_mode_enabled) {
    pdp8_api_request_interrupt(cpu, PDP8_KL8E_KEYBOARD_DEVICE_CODE);
}
```

### Step 3: Update Config Parser
Update factory/driver.py to parse new interrupt mode options for each device

### Step 4: Testing & Documentation
- Create test programs for each device's interrupt mode
- Document ISR patterns for multi-device polling
- Performance comparison: polled vs interrupt-driven

## Testing Current Watchdog Interrupts

To test the existing watchdog interrupt support:

```asm
/ Simple watchdog interrupt test
*0000
        JMP START

*0006   / Reserved for context save
        0
        0
        0

*0010   / Interrupt Service Routine
INTR,   CLA CLL         / Clear AC and Link
        ISK 6551        / Check watchdog flag
        JMP EXIT        / Not watchdog, exit
        IOT 6551 5      / Clear watchdog flag (operation 5 = read/restart)
        TAD ONE         / Increment counter
        DCA COUNT       / Store it
EXIT,   ION             / Re-enable interrupts
        JMP I 0x0007    / Return to interrupted program

*0020
START,  TAD WDVALUE     / Load watchdog timer value
        IOT 6551 1      / Start watchdog (operation 1 = write control)
        ION             / Enable interrupts
        JMP LOOP        / Main program loop

LOOP,   JMP LOOP        / Busy wait for interrupt
        
*0100
ONE,    1
COUNT,  0
WDVALUE, 0o0305         / 5 deciseconds (0.5 seconds), interrupt one-shot mode
```

Configuration:
```
device watchdog {
  enabled = true
  mode = interrupt        # Enable interrupt mode
  periodic = false        # One-shot (not periodic)
  default_count = 5       # 5 deciseconds = 0.5 seconds
}
```

## Summary

| Aspect | Watchdog | Others |
|--------|----------|--------|
| Can raise interrupts? | ✅ YES | ❌ No (skip-only) |
| Has flags? | ✅ YES (expiry) | ✅ YES (ready/error) |
| Has ISK? | ❌ NO | ✅ YES (all 5) |
| Config option? | ✅ YES | ❌ Not yet |
| Phase 2 task? | N/A | Add interrupt modes |

**Conclusion**: Phase 1 provides a working interrupt system with watchdog. Phase 2 will extend it to keyboard, printer, and magtape devices. All infrastructure is in place - just need config options and two-line code additions to each device.

---

**Report**: INTERRUPT_SOURCES.md  
**Date**: 2025-10-30  
**Status**: COMPLETE ✅
