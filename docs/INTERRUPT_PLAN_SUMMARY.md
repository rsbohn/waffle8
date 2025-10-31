# Interrupt Support Implementation: Summary

## Decision: Single Line Interrupt Model

After review, we're implementing a **single interrupt line** design with **software device polling** via skip chains. This matches real PDP-8 hardware and is much simpler than per-device queues.

**Why**:
- ✅ Authentic to real PDP-8 hardware
- ✅ Simpler implementation (counter, not queue)
- ✅ No queue overflow concerns
- ✅ Software has full flexibility in polling order via skip chain
- ✅ Natural fit with existing ISK (interrupt skip) instructions

---

## Architecture Overview

### State Changes to `struct pdp8` (src/emulator/main.c)

**ADD**:
```c
bool interrupt_enable;      /* Set by ION, cleared by IOFF */
int interrupt_pending;      /* Count of devices requesting interrupt */
```

**REMOVE**: Nothing - these are additions only.

### Three Key Operations

#### 1. ION/IOFF Instruction Decoding
**Location**: `operate_group2()` in `main.c`

```c
if (instruction & 0x0001u) {    /* ION instruction */
    cpu->interrupt_enable = true;
}
/* IOFF is implicit (bit 0 not set) */
```

#### 2. Interrupt Request from Device
**Location**: Device handlers (watchdog.c, kl8e_console.c, etc.)

```c
if (device_event_occurred) {
    pdp8_api_request_interrupt(cpu, device_code);
    // ↓ This increments: cpu->interrupt_pending++
}
```

#### 3. Interrupt Dispatch in Step
**Location**: `pdp8_api_step()` in `main.c` (after apply_skip)

```c
if (cpu->interrupt_enable && cpu->interrupt_pending > 0) {
    /* Save context */
    cpu->memory[0x0006] = cpu->ac;
    cpu->memory[0x0007] = cpu->pc;
    cpu->memory[0x0008] = (uint16_t)cpu->link;
    
    /* Mark one interrupt being serviced */
    cpu->interrupt_pending--;
    
    /* Prevent recursion */
    cpu->interrupt_enable = false;
    
    /* Jump to ISR */
    cpu->pc = normalise_address(cpu, 0x0010);
}
```

---

## API Functions

### src/emulator/pdp8.h

```c
/* Request interrupt from device (device_code for logging/polling)
 * Returns 0 on success, -1 if device_code out of range
 */
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code);

/* Check if interrupt pending (non-blocking)
 * Returns 1 if pending > 0, else 0
 */
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);

/* Manually decrement pending count
 * Returns new count, -1 on error
 * (mainly for testing/cleanup)
 */
int pdp8_api_clear_interrupt_pending(pdp8_t *cpu);
```

---

## Device Interrupt Identification: Skip Chain

Since there's only ONE interrupt line, the ISR must figure out which device(s) interrupted:

### Example: ISR at 0x0010

```asm
        * 0010
INTR,   CLA CLL
        
        ISK 6031        ; Does keyboard want service?
        JMP CHK2        ; No - check next device
        JMS HANDLE_KBD  ; Yes - handle it
        
CHK2,   ISK 6041        ; Does printer want service?
        JMP CHK3        ; No - check next device
        JMS HANDLE_PRINTER
        
CHK3,   ISK 6551        ; Does watchdog want service?
        JMP DONE        ; No devices to service
        JMS HANDLE_WD
        
DONE,   / Restore and return
        CLA
        TAD 0006        ; Get saved AC
        DCA TEMP
        TAD 0007        ; Get saved PC
        DCA 0011
        
        ION             ; Re-enable interrupts
        JMP I 0011      ; Return to interrupted code
```

**ISK instructions**:
- Each device provides its own ISK (e.g., 6031 for keyboard)
- ISK checks device's pending flag and skips next instruction if set
- ISK clears the flag (so device knows we saw it)

---

## Watchdog Integration

### watchdog.h: Add Commands

```c
enum watchdog_cmd {
    WD_CMD_DISABLE = 0,
    WD_CMD_RESET_ONE_SHOT = 1,
    WD_CMD_RESET_PERIODIC = 2,
    WD_CMD_HALT_ONE_SHOT = 3,
    WD_CMD_HALT_PERIODIC = 4,
    WD_CMD_INTERRUPT_ONE_SHOT = 5,    /* NEW */
    WD_CMD_INTERRUPT_PERIODIC = 6,    /* NEW */
};
```

### watchdog.c: Modify Expiry Handler

```c
case WD_CMD_INTERRUPT_ONE_SHOT:
case WD_CMD_INTERRUPT_PERIODIC:
    pdp8_api_request_interrupt(cpu, PDP8_WATCHDOG_DEVICE_CODE);
    if (wd->cmd == WD_CMD_INTERRUPT_ONE_SHOT) {
        wd->enabled = 0;
    } else {
        wd->remaining_ds = wd->configured_count; /* restart */
    }
    break;
```

---

## Monitor Configuration

### Already Supported

In `monitor_config.h`, the field already exists:
```c
char *watchdog_mode;  /* "halt", "reset", "interrupt" */
```

### Update Parsing

In `monitor_platform_posix.c`:
- Parse `watchdog.mode = "interrupt"` 
- Pass to watchdog setup (already handled by watchdog.c checking the mode)

---

## Implementation Checklist

### Phase 1: Core (main.c)
- [ ] Add `interrupt_enable` and `interrupt_pending` fields to struct pdp8
- [ ] Update `pdp8_api_reset()` to initialize both to false/0
- [ ] Implement ION/IOFF in `operate_group2()`
- [ ] Add interrupt dispatch in `pdp8_api_step()` after apply_skip()
- [ ] Implement three API functions

### Phase 2: Watchdog
- [ ] Add interrupt commands to watchdog.h
- [ ] Update watchdog.c expiry handler

### Phase 3: Testing
- [ ] Unit tests (test_ion_ioff, test_interrupt_pending, test_dispatch)
- [ ] Watchdog interrupt tests
- [ ] Assembly example: demo/interrupt-test.asm

### Phase 4: Documentation
- [ ] Add comments to API functions
- [ ] Create interrupt handler example
- [ ] Update AGENTS.md with new conventions

---

## Key Advantages vs Complex Queue

| Aspect | Count-Based (New) | Queue-Based (Rejected) |
|--------|------------------|----------------------|
| **Memory** | 4 bytes (counter) | 128+ bytes (queue + ptrs) |
| **Complexity** | Simple (++/--)| Complex (wrap around, enqueue/dequeue) |
| **Matches PDP-8** | Yes ✅ | No |
| **Device polling** | ISK skip chain | Hard to implement per-device |
| **Multiple device handling** | ISR loops through ISK chain | Dequeue order matters |

---

## Context Save/Restore Layout

Hardware saves/ISR restores:

```
Address | Size | Purpose
0x0006  | 12b  | Interrupted AC
0x0007  | 12b  | Interrupted PC  
0x0008  | 1b   | Interrupted LINK
```

ISR typically:
```asm
/ Save context (already done by hardware)
TAD 0006        ; Get AC
DCA AC_SAVE     ; Save to ISR workspace if needed

/ ... service device ...

/ Restore context
TAD 0006        ; Get AC back
DCA TEMP
TAD 0007        ; Get PC
DCA 0011        ; Temp location for return

ION             ; Re-enable interrupts
JMP I 0011      ; Resume interrupted code
```

---

## Files to Modify

| File | Changes | Lines |
|------|---------|-------|
| `src/emulator/main.c` | Add state fields, ION/IOFF, dispatch | ~30 |
| `src/emulator/pdp8.h` | Add three API functions | ~15 |
| `src/emulator/watchdog.h` | Add interrupt commands | ~3 |
| `src/emulator/watchdog.c` | Update expiry handler | ~10 |
| `src/monitor_platform_posix.c` | Parse "interrupt" mode | Already supported |
| `tests/test_emulator.c` | Add interrupt tests | ~50 |
| `factory/test_watchdog.py` | Add interrupt tests | ~30 |
| `demo/interrupt-test.asm` | Example ISR | ~50 |

**Total**: ~200 lines of code across 8 files

---

## Testing Strategy

### Unit Tests (C)
```
test_ion_ioff()          - Verify ION/IOFF bits toggle flag
test_interrupt_pending() - Request/peek pending count  
test_interrupt_dispatch()- Full dispatch cycle
test_watchdog_interrupt()- Watchdog with interrupt mode
```

### Integration Tests (Python)
```
test_interrupt_one_shot()  - Watchdog one-shot mode
test_interrupt_periodic()  - Watchdog periodic mode
```

### Assembly Test
```
demo/interrupt-test.asm
- Simple ION execution
- Request interrupt via watchdog
- ISR with skip chain
- Return and resume
```

---

## Next Steps

1. **Implement core** (main.c state + ION/IOFF + dispatch)
2. **Add API functions** (request, peek, clear)
3. **Watchdog integration** (commands + expiry handler)
4. **Write tests** (unit + integration)
5. **Create example** (demo/interrupt-test.asm)
6. **Documentation** (comments + guide)

Total effort: ~1-2 hours implementation, 1 hour testing, 1 hour documentation

---

## References

- [Interrupt Implementation Plan](docs/interrupt-implementation-plan.md) - Full technical design
- [Skip Chain Polling](docs/interrupt-skip-chain-polling.md) - Device identification patterns
- [Flow Diagrams](docs/interrupt-flow-diagram.md) - Visual state transitions
- [Queue Rationale](docs/interrupt-queue-rationale.md) - Why we *didn't* use a queue

