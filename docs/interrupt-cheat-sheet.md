# Interrupt Support — Developer Cheat Sheet

## Quick Facts

| Item | Value |
|------|-------|
| **Interrupt Enable Flag** | `cpu->interrupt_enable` (bool) |
| **Queue Capacity** | 64 entries |
| **Queue Full Behavior** | pdp8_api_request_interrupt() returns -1 |
| **Context Save Addresses** | 0x0006 (AC), 0x0007 (PC), 0x0008 (LINK) |
| **Trap Vector Base** | 0x0010 + trap_vector_offset |
| **ION Opcode** | 0o6001 = 0x0E01 (Group 2 + bit 0) |
| **IOFF Opcode** | 0o6000 = 0x0E00 (Group 2 bit 0 clear) |
| **Watchdog Device Code** | 55 (0o67) |
| **Interrupt Dispatch Point** | End of pdp8_api_step(), after apply_skip() |
| **Interrupt Disable Timing** | Automatic on dispatch (prevents recursion) |

## Code Snippets

### Enable/Disable Interrupts
```asm
        ION             ; Enable interrupts (6001)
        IOFF            ; Disable interrupts (6000)
```

### Request Interrupt (C API)
```c
pdp8_api_request_interrupt(cpu, 55, 0);  /* Device 55 (watchdog), vector 0 */
```

### Check Interrupt Pending
```c
if (pdp8_api_peek_interrupt_pending(cpu)) {
    /* Queue is not empty */
}
```

### Minimal ISR Handler (Assembly)
```asm
        * 0010          ; Trap vector address (standard)
ISR,    CLA             ; Start handler
        / ... handle interrupt ...
        CLA
        TAD 0007        ; Get saved PC
        DCA 0011        ; Temp storage
        ION             ; Re-enable interrupts
        JMP I 0011      ; Return
```

## File Map

```
Implementation Files:
├─ src/emulator/main.c           Main CPU + interrupt dispatch
├─ src/emulator/pdp8.h           Public API
├─ src/emulator/watchdog.c       Watchdog interrupt support
├─ src/monitor_platform_posix.c  Config parsing
├─ tests/test_emulator.c         Unit tests
├─ factory/test_watchdog.py      Integration tests
└─ demo/interrupt-test.asm       Example program

Documentation:
├─ interrupt-implementation-plan.md   Full spec (11 sections)
├─ interrupt-quick-reference.md       API & files summary
├─ interrupt-flow-diagram.md          8 diagrams
├─ INTERRUPT-PLAN-SUMMARY.md         Executive summary
└─ interrupt-cheat-sheet.md          This file
```

## struct pdp8 Fields to Add

```c
/* In struct pdp8 { ... } */
bool interrupt_enable;                    /* ION/IOFF flag */
uint16_t interrupt_queue[64];             /* FIFO buffer */
size_t interrupt_queue_head;              /* Write pointer */
size_t interrupt_queue_tail;              /* Read pointer */
```

## Function Signatures

```c
/* In pdp8.h */
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code, uint8_t trap_vector);
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);
int pdp8_api_clear_interrupt_queue(pdp8_t *cpu);
```

## Interrupt Dispatch Pseudocode

```
In pdp8_api_step(), after apply_skip():

if (cpu->interrupt_enable && cpu->interrupt_queue_head != cpu->interrupt_queue_tail) {
    entry = cpu->interrupt_queue[cpu->interrupt_queue_tail];
    cpu->interrupt_queue_tail = (cpu->interrupt_queue_tail + 1) % 64;
    
    device_code = (entry >> 8) & 0xFF;
    trap_vector = entry & 0xFF;
    
    cpu->memory[0x0006] = cpu->ac;          /* Save AC */
    cpu->memory[0x0007] = cpu->pc;          /* Save PC */
    cpu->memory[0x0008] = cpu->link;        /* Save LINK */
    
    cpu->interrupt_enable = false;          /* Disable to prevent nesting */
    
    cpu->pc = normalise_address(cpu, 0x0010 + trap_vector);
}
```

## ION/IOFF Decode in operate_group2()

```c
/* In operate_group2(pdp8_t *cpu, uint16_t instruction) */
if (instruction & 0x0001u) { /* ION */
    cpu->interrupt_enable = true;
}
/* IOFF is implicit (bit not set) */
```

## Watchdog Interrupt Commands

```c
/* In watchdog.h */
#define WD_CMD_INTERRUPT_ONE_SHOT 5
#define WD_CMD_INTERRUPT_PERIODIC 6

/* In watchdog.c expiry handler */
case WD_CMD_INTERRUPT_ONE_SHOT:
case WD_CMD_INTERRUPT_PERIODIC:
    pdp8_api_request_interrupt(cpu, PDP8_WATCHDOG_DEVICE_CODE, 0);
    if (wd->cmd == WD_CMD_INTERRUPT_ONE_SHOT) {
        wd->enabled = 0;
    } else {
        wd->remaining_ds = wd->configured_count;  /* Restart */
    }
    break;
```

## Monitor Config Parsing

```c
/* In monitor_platform_posix.c, config file [watchdog] section */
} else if (strcmp(key, "mode") == 0) {
    if (strcmp(value, "interrupt") == 0) {
        config->watchdog_mode = strdup("interrupt");
    } else if (strcmp(value, "halt") == 0) {
        config->watchdog_mode = strdup("halt");
    } else if (strcmp(value, "reset") == 0) {
        config->watchdog_mode = strdup("reset");
    }
}
```

## Test Checklist

### Unit Tests (tests/test_emulator.c)
- [ ] test_ion_ioff() - ION sets flag, IOFF clears it
- [ ] test_interrupt_request() - Enqueue works
- [ ] test_interrupt_dispatch() - Full cycle
- [ ] test_interrupt_context_save() - Memory[0x0006-0x0008] correct
- [ ] test_interrupt_queue_multiple() - Multiple entries work
- [ ] test_watchdog_interrupt() - Watchdog mode integration

### Integration Tests (factory/test_watchdog.py)
- [ ] test_ion_ioff_instruction() - Via ctypes
- [ ] test_interrupt_mode_one_shot() - Watchdog one-shot
- [ ] test_interrupt_mode_periodic() - Watchdog periodic

### Manual Tests
- [ ] demo/interrupt-test.asm compiles and runs
- [ ] No regression in existing tests
- [ ] Backward compatible with old code

## Common Pitfalls

| Issue | Solution |
|-------|----------|
| Queue full | Check return value of pdp8_api_request_interrupt() |
| ISR not triggered | Verify interrupt_enable was set via ION |
| Wrong ISR called | Check trap_vector parameter (0-7 typically) |
| Nested interrupt | Dispatch sets interrupt_enable=false automatically |
| Context corruption | Save area is fixed at 0x0006-0x0008, don't use for other purposes |
| IOFF doesn't work | IOFF is implicit (Group 2 operate without bit 0) |

## Performance Notes

- **Interrupt check**: O(1) - single flag and queue check
- **Enqueue**: O(1) - circular buffer append
- **Dequeue**: O(1) - circular buffer read
- **Dispatch overhead**: ~100 cycles (save AC/PC/LINK, jump) per interrupt
- **No penalty for disabled interrupts**: Just checks flag at end of cycle

## Compatibility Matrix

| Feature | Old Code | New Code |
|---------|----------|----------|
| No ION/IOFF | ✅ Works | ✅ Works (disabled by default) |
| With ION/IOFF | ❌ N/A | ✅ Works |
| Watchdog halt mode | ✅ Works | ✅ Works (default) |
| Watchdog interrupt mode | ❌ N/A | ✅ New |
| IOT device skip | ✅ Works | ✅ Works (unchanged) |
| Tick handlers | ✅ Works | ✅ Works (unchanged) |

## References

- **PDP-8 Handbook**: Interrupt control instructions (ION/IOFF)
- **waffle8 AGENTS.md**: Project structure guidelines
- **Our Plan**: interrupt-implementation-plan.md (full spec)

---

**Last Updated**: October 30, 2025  
**Status**: Ready for implementation  
**Quick Check**: 2-3 hours for basic implementation  
**Full Implementation**: 8-10 hours with tests
