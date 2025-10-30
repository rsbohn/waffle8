# Interrupt Support Implementation Plan — Executive Summary

## Problem Statement
The waffle8 PDP-8 emulator currently lacks hardware interrupt support. Devices can only notify the CPU via skip instructions or halt/reset commands. This limits simulation of real-world interrupt-driven PDP-8 software and prevents implementation of advanced watchdog modes.

## Solution Overview
Add complete PDP-8 hardware interrupt support comprising:
1. **Interrupt state management** in the CPU
2. **ION/IOFF control instructions** for enabling/disabling interrupts  
3. **Interrupt service dispatch** with proper context save/restore
4. **Device integration** (starting with watchdog)
5. **Monitor system configuration** support

## Key Components

### 1. CPU State (struct pdp8)
**Add 3 fields**:
- `bool interrupt_enable` - ION/IOFF flag
- `uint16_t interrupt_queue[64]` - FIFO queue
- `size_t interrupt_queue_head/tail` - Queue pointers

**Default**: interrupts **disabled**, queue **empty** (backward compatible)

### 2. ION/IOFF Instructions  
**Where**: Group 2 Operate instructions (opcode 0x0E)

**Implementation**: Single bit check in `operate_group2()`:
```c
if (instruction & 0x0001u) cpu->interrupt_enable = true;  /* ION */
```

**Opcodes**:
- ION (octal 6001) = 0x0E01
- IOFF (octal 6000) = 0x0E00

### 3. Interrupt Dispatch
**When**: End of every instruction cycle in `pdp8_api_step()`

**Action**: If `interrupt_enable && queue_not_empty`:
1. Dequeue interrupt request
2. Save AC, PC, LINK to fixed addresses (0x0006-0x0008)
3. Disable interrupts (`interrupt_enable = false`)
4. Jump to trap vector (0x0010 + offset)

### 4. API Functions (New)
```c
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code, uint8_t trap_vector);
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);
int pdp8_api_clear_interrupt_queue(pdp8_t *cpu);
```

### 5. Device Integration
**Watchdog device** - Add interrupt commands:
- `WD_CMD_INTERRUPT_ONE_SHOT = 5`
- `WD_CMD_INTERRUPT_PERIODIC = 6`

When watchdog expires in interrupt mode, it calls `pdp8_api_request_interrupt()` instead of halt/reset.

### 6. Monitor Configuration
**Already has**: `char *watchdog_mode;` field in config

**Just needs**: Parser update to recognize `"interrupt"` value (like existing `"halt"` and `"reset"`)

## Implementation Scope

### Files to Modify

| File | Changes | Effort |
|------|---------|--------|
| `src/emulator/main.c` | Add state fields, ION/IOFF logic, interrupt dispatch | **2-3 hours** |
| `src/emulator/pdp8.h` | Export 3 new API functions with docs | **30 min** |
| `src/emulator/watchdog.c` | Add 2 interrupt commands, modify expiry handler | **1 hour** |
| `src/monitor_platform_posix.c` | Parse interrupt mode in config | **30 min** |
| `tests/test_emulator.c` | 4-5 interrupt unit tests | **2 hours** |
| `factory/test_watchdog.py` | 2-3 interrupt integration tests | **1 hour** |
| `demo/interrupt-test.asm` | Example interrupt handler | **1 hour** |
| `docs/` | Already created (3 files) | **Done** |

**Total Estimated Effort**: 8-10 developer hours

### Backward Compatibility
✅ **Full compatibility maintained**:
- Interrupts disabled by default
- No changes to existing API/ABI
- Default watchdog mode unchanged ("halt")
- Existing code unaffected

## Design Highlights

### Interrupt Queue
- **FIFO design**: Simple and efficient
- **64 entries**: Sufficient for demo/test use
- **Format**: `(device_code << 8) | trap_vector`
- **Blocking when full**: pdp8_api_request_interrupt() returns error

### Context Save
- **Fixed addresses**: 0x0006-0x0008 (AC, PC, LINK)
- **Simple model**: Assumes single interrupt context
- **Software manageable**: ISR can save to stack if needed

### Interrupt Enable
- **One flag**: `interrupt_enable` (true/false)
- **Set by ION**: Bit 0 of Group 2 Operate instruction
- **Cleared on dispatch**: Prevents recursive interrupts
- **Restored by ISR**: ISR executes ION to re-enable

## Interrupt Flow (Simplified)

```
┌─────────────────────────────────────────┐
│ Device triggers (e.g., watchdog expiry) │
└──────────────┬──────────────────────────┘
               │
               ▼
       pdp8_api_request_interrupt()
               │
               ▼
    ┌──────────────────────────────┐
    │ Enqueue (device, vector)     │
    │ in interrupt_queue[]         │
    └──────────────┬───────────────┘
                   │
         (wait for ION or until
          interrupts already enabled)
                   │
                   ▼
    ┌──────────────────────────────┐
    │ End of instruction cycle     │
    │ (in pdp8_api_step)           │
    │ - Check interrupt_enable     │
    │ - Check queue not empty      │
    └──────────────┬───────────────┘
                   │
                   ▼
    ┌──────────────────────────────┐
    │ Dequeue interrupt request    │
    │ Save AC, PC, LINK            │
    │ Disable interrupts           │
    │ Jump to trap vector          │
    └──────────────┬───────────────┘
                   │
                   ▼
    ┌──────────────────────────────┐
    │ Execute interrupt handler    │
    │ at trap vector address       │
    │ (e.g., 0x0010)              │
    └──────────────┬───────────────┘
                   │
                   ▼
    ┌──────────────────────────────┐
    │ ISR restores context and     │
    │ executes ION to re-enable    │
    └──────────────┬───────────────┘
                   │
                   ▼
    ┌──────────────────────────────┐
    │ Return to interrupted code   │
    │ (or dispatch next interrupt) │
    └──────────────────────────────┘
```

## Testing Strategy

### Unit Tests (C)
```c
test_ion_ioff()              /* ION/IOFF instruction decode */
test_interrupt_request()     /* Request and queue mechanics */
test_interrupt_dispatch()    /* Full dispatch cycle */
test_interrupt_context_save()/* Verify memory[0x0006-0x0008] */
test_interrupt_queue_full()  /* Behavior when queue full */
test_watchdog_interrupt()    /* Watchdog in interrupt mode */
```

### Integration Tests (Python)
```python
test_ion_ioff_instruction()           /* Via ctypes binding */
test_interrupt_mode_one_shot()        /* Watchdog one-shot */
test_interrupt_mode_periodic()        /* Watchdog periodic */
test_interrupt_handler_roundtrip()    /* Full ISR test */
```

### Example Programs
- `demo/interrupt-test.asm` - Simple interrupt handler with ION/IOFF usage

## Known Limitations (Future Work)

1. **Single interrupt context** - No nested interrupts without ISR stack management
2. **No priority levels** - Simple FIFO queue (real PDP-8 had 4 priorities)
3. **No per-device masks** - Cannot selectively disable device interrupts
4. **No vector table** - All devices use same trap vector pool

## Success Criteria

✅ **Completion when**:
- [ ] ION/IOFF instructions execute correctly
- [ ] Interrupt requests are queued and dispatched
- [ ] Context is saved/restored properly
- [ ] Watchdog can trigger interrupts
- [ ] Monitor config supports interrupt mode
- [ ] All unit tests pass
- [ ] Integration tests pass
- [ ] Example program runs correctly
- [ ] No regression in existing tests
- [ ] Documentation complete

## Related Documentation

- **Full Implementation Plan**: `docs/interrupt-implementation-plan.md` (11 sections)
- **Quick Reference**: `docs/interrupt-quick-reference.md` (API summary, files to modify)
- **Flow Diagrams**: `docs/interrupt-flow-diagram.md` (8 detailed diagrams)
- **This Summary**: `docs/INTERRUPT-PLAN-SUMMARY.md`

## Next Steps

1. **Review** this plan with team/stakeholders
2. **Start Phase 1** (Core mechanics in main.c)
3. **Unit test** each phase incrementally
4. **Integrate** watchdog support
5. **Test** with example programs
6. **Document** completion with devlog entry

## Questions & Contact

For questions about this plan, refer to:
- Architecture details → `interrupt-implementation-plan.md`
- Quick lookup → `interrupt-quick-reference.md`  
- Visual aids → `interrupt-flow-diagram.md`
- Implementation tasks → `manage_todo_list` in this session

---

**Plan Created**: October 30, 2025  
**Status**: Ready for implementation  
**Estimated Duration**: 8-10 developer hours  
**Risk Level**: Low (fully backward compatible)
