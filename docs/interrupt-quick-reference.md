# Interrupt Support: Quick Reference

## Summary
This plan adds hardware interrupt support to the PDP-8 emulator through three main components:

### 1. State Management
**Where**: `struct pdp8` in `src/emulator/main.c`

**What to add**:
- `bool interrupt_enable` - ION/IOFF control flag
- `int interrupt_pending` - Count of pending interrupt requests

**Why**: Devices can request interrupts even when interrupts are disabled; the counter tracks how many devices are waiting to be serviced. Single interrupt line design—ISR uses skip chain (ISK) to identify which device(s) need service.

### 2. ION/IOFF Instructions
**Where**: `operate_group2()` function in `src/emulator/main.c`

**What to add**:
```c
if (instruction & 0x0001u) { /* ION */
    cpu->interrupt_enable = true;
}
```

**Why**: PDP-8 software uses ION to enable interrupts and IOFF (implicit when bit is clear) to disable them. This is the only way interrupt-driven code can control when interrupts are processed.

**Opcode mapping**:
- `ION` (octal 6001) = 0x0E01 - Group 2 operate with bit 0 set
- `IOFF` (octal 6000) = 0x0E00 - Group 2 operate with bit 0 clear

### 3. Interrupt Service Dispatch
**Where**: End of `pdp8_api_step()` in `src/emulator/main.c`, after `apply_skip()`

**What to add**:
```c
/* Check if interrupts enabled and one or more devices pending */
if (cpu->interrupt_enable && cpu->interrupt_pending > 0) {
    /* Save AC, PC, LINK to fixed addresses 0x0006-0x0008 */
    /* Decrement interrupt_pending counter (one being serviced) */
    /* Disable interrupts to prevent nesting */
    /* Jump to single trap vector at 0x0010 */
}
```

**Why**: When an interrupt is pending and interrupts are enabled, the CPU must:
1. Save the interrupted program's state (AC, PC, LINK)
2. Disable interrupts (to prevent recursive nesting)
3. Jump to the interrupt service routine at 0x0010
4. Software (ISR) uses skip chain to identify which device(s) need service

**Device Identification**: ISR polls each device using ISK instructions (skip if device has pending flag)

## API Functions to Add

### In `src/emulator/pdp8.h`:
```c
/* Request an interrupt from a device.
 * device_code: 0-63, identifies device (used for logging/debugging)
 */
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code);

/* Check if interrupt is pending */
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);

/* Decrement pending interrupt count (called by ISR or for cleanup) */
int pdp8_api_clear_interrupt_pending(pdp8_t *cpu);
```

### In `src/emulator/main.c`:
- Implement the three functions above (simple increment/decrement of counter)
- Dispatch logic in `pdp8_api_step()` - single trap vector at 0x0010

## Integration Points

### Watchdog Device (`src/emulator/watchdog.c`)
**Add new commands**:
- `WD_CMD_INTERRUPT_ONE_SHOT = 5`
- `WD_CMD_INTERRUPT_PERIODIC = 6`

**Change watchdog expiry handler** to call `pdp8_api_request_interrupt()` instead of `pdp8_api_set_halt()` when in interrupt mode.

### Monitor Configuration (`src/monitor_config.h` + `src/monitor_platform_posix.c`)
**Already has**: `char *watchdog_mode; /* "halt", "reset", "interrupt" */`

**Needs**: Parser logic to recognize "interrupt" mode and pass it to watchdog setup.

## Standard Interrupt Flow (ISR Pattern)

### ISR Entry (at address 0x0010):
```asm
INTR,   CLA CLL
        
        / Poll devices using skip chain
        ISK 6031        / Check keyboard?
        JMP CHK2        / No
        JMS HANDLE_KBD  / Yes - handle it
        
CHK2,   ISK 6041        / Check printer?
        JMP CHK3        / No
        JMS HANDLE_PRINTER
        
CHK3,   ISK 6551        / Check watchdog?
        JMP DONE        / No
        JMS HANDLE_WD   / Yes - handle it
```

### ISR Epilogue (Return from Interrupt):
```asm
DONE,   CLA
        TAD 0006        / Restore AC
        DCA TEMP
        TAD 0007        / Restore PC  
        DCA 0011        / Return address
        
        ION             / Re-enable interrupts
        JMP I 0011      / Return to interrupted code
```

**Key**: ISR must use skip chain (ISK) to identify which device(s) need service. Single interrupt line means multiple devices could interrupt simultaneously.

## Testing Strategy

### Unit Tests (C):
- `test_ion_ioff()` - Verify ION/IOFF bit handling
- `test_interrupt_request_queue()` - Enqueue/dequeue mechanics
- `test_interrupt_dispatch()` - Full dispatch cycle
- `test_watchdog_interrupt()` - Watchdog in interrupt mode

### Integration Tests (Python):
- `test_interrupt_mode_one_shot()` - Watchdog one-shot mode
- `test_interrupt_mode_periodic()` - Watchdog periodic mode
- `test_ion_ioff_instruction()` - Via ctypes bindings

### Assembly Programs:
- `demo/interrupt-test.asm` - Simple interrupt handler example

## Files to Modify

| File | Changes | Complexity |
|------|---------|-----------|
| `src/emulator/main.c` | Add state, ION/IOFF decode, interrupt dispatch | **High** |
| `src/emulator/pdp8.h` | Add interrupt API functions | **Low** |
| `src/emulator/watchdog.c` | Add interrupt commands, modify expiry handler | **Medium** |
| `src/monitor_config.h` | Already has mode field | **None** |
| `src/monitor_platform_posix.c` | Add interrupt mode parser | **Low** |
| `tests/test_emulator.c` | Add interrupt tests | **High** |
| `factory/test_watchdog.py` | Add interrupt mode tests | **Medium** |

## Backward Compatibility

✅ **Guaranteed**:
- Interrupts disabled by default on CPU reset
- No changes to existing API functions
- Existing code without ION/IOFF unaffected
- Watchdog default mode remains "halt"

## Implementation Order

1. **Core mechanics** (main.c state + dispatch)
2. **ION/IOFF** decoding (operate_group2)
3. **API functions** (pdp8_api_request_interrupt, etc.)
4. **Watchdog integration** (watchdog.c interrupt support)
5. **Monitor config** (config parsing)
6. **Tests** (unit + integration + examples)

