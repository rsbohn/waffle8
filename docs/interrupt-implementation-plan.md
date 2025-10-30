# Hardware Interrupt Support Implementation Plan

## Overview
This document outlines the implementation of PDP-8 hardware interrupt support in the waffle8 emulator. The goal is to add proper interrupt state management, ION/IOFF control instructions, and interrupt service routine dispatch, while maintaining backward compatibility with existing code.

---

## 1. State Management Architecture

### 1.1 CPU State Extensions
Add the following fields to `struct pdp8` in `src/emulator/main.c`:

```c
struct pdp8 {
    // ...existing fields...
    
    /* Interrupt control */
    bool interrupt_enable;           /* ION enables, IOFF disables */
    int interrupt_pending;           /* Count of pending interrupt requests */
};
```

### 1.2 Interrupt Semantics
- **Single Interrupt Line**: Like real PDP-8 hardware with a single interrupt input
- **Pending Counter**: Tracks how many devices have requested interrupt
  - Incremented by `pdp8_api_request_interrupt(cpu, device_code)`
  - Decremented when interrupt is dispatched in `pdp8_api_step()`
- **Device Identification**: Software uses skip chain (ISK instructions) to poll devices and determine which ones need service

- **Standard Trap Vector** (by convention):
  - 0x0010: Interrupt service routine entry point
  - ISR polls devices via skip instructions (ISK from each device)
  - ISR determines which device(s) to service

### 1.3 Interrupt Sequence
1. Device requests interrupt via `pdp8_api_request_interrupt(cpu, device_code)`
   - Increments `interrupt_pending` counter
   - Request is buffered if `interrupt_enable == false` (deferred delivery)
2. At end of instruction cycle in `pdp8_api_step()`:
   - If `interrupt_enable == true` AND `interrupt_pending > 0`:
     - Save AC, PC, LINK to fixed save area (0x0006, 0x0007, 0x0008)
     - Decrement `interrupt_pending` counter
     - Disable interrupts (`interrupt_enable = false`)
     - Set PC to trap vector address (0x0010)
3. Interrupt Service Routine (in software):
   - Restores minimal context as needed
   - Polls devices using ISK skip chain to identify requestor(s)
   - Services the device(s)
   - Restores full context (AC, PC, LINK) from save area
   - Executes ION to re-enable interrupts
   - Returns to interrupted code

---

## 2. ION/IOFF Instruction Implementation

### 2.1 Instruction Format
PDP-8 Group 2 Operate instructions (opcode 0x0E00):

```
         ION  (Interrupt ON):  Bit 0 set in Operate instruction
         IOFF (Interrupt OFF): Bit 0 clear (part of group 2)
```

More precisely:
- **ION** (octal 6001): Group 2 operate with bit 0 set
  - Encoding: 0x0E00 | 0x0001 = 0x0E01
- **IOFF** (octal 6000): Group 2 operate with bit 0 clear  
  - Encoding: 0x0E00 | 0x0000 = 0x0E00

### 2.2 Implementation in `operate_group2()`
In `main.c`, extend `operate_group2()` function:

```c
static void operate_group2(pdp8_t *cpu, uint16_t instruction) {
    // ...existing code for CLA, SMA, SZA, SNL, OSR, HLT...
    
    if (instruction & 0x0001u) { /* ION */
        cpu->interrupt_enable = true;
    }
    /* IOFF is implicit when 0x0001 is not set in group 2 operates */
}
```

**Note**: IOFF is already implicitly handled by NOT setting the bit. Group 2 operates that clear all bits (0x0E00) effectively disable interrupts.

### 2.3 Interrupt Disable/Enable Sequences
The standard PDP-8 pattern to save and restore interrupt state:

```asm
/ Disable interrupts and save old state
CLA
TAD ZERO
IOFF                    ; bit 3 of group 2 operates controls this
SPA
JMP INTERRUPTS_WERE_ON
CLA
TAD NEG_ONE
DCA OLD_INT_STATE
JMP GOT_STATE
INTERRUPTS_WERE_ON,
CLA
TAD ZERO
DCA OLD_INT_STATE
GOT_STATE,

/ ... critical section ...

/ Restore interrupt state
CLA
TAD OLD_INT_STATE
SZA
JMP RESTORE_ON
IOFF                    ; they were off
JMP DONE
RESTORE_ON,
ION                     ; they were on
DONE,
```

---

## 3. Interrupt Request and Service

### 3.1 New API Functions in `pdp8.h`

```c
/* Request an interrupt from a device.
 * device_code: 0-63, identifies the device (used by ISR for polling)
 * Returns 0 on success, -1 on error
 */
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code);

/* Check if an interrupt is pending (non-blocking)
 * Returns 1 if interrupt_pending > 0, 0 otherwise
 */
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);

/* Manually decrement pending interrupt count
 * Returns new count, or -1 on error
 */
int pdp8_api_clear_interrupt_pending(pdp8_t *cpu);
```

### 3.2 Interrupt Pending Counter Implementation
In `main.c`:

```c
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code) {
    if (!cpu || device_code >= 64u) {
        return -1;
    }
    
    /* Simply increment the pending counter.
     * Real PDP-8 hardware would assert a physical interrupt line;
     * the counter simulates "how many devices want interrupts now".
     */
    cpu->interrupt_pending++;
    
    /* Note: device_code is passed for reference but not stored.
     * Software uses skip chain (ISK instructions) to identify
     * which device(s) actually need service.
     */
    return 0;
}

int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu) {
    return (cpu && cpu->interrupt_pending > 0) ? 1 : 0;
}

int pdp8_api_clear_interrupt_pending(pdp8_t *cpu) {
    if (!cpu) return -1;
    if (cpu->interrupt_pending > 0) {
        cpu->interrupt_pending--;
    }
    return cpu->interrupt_pending;
}
```

### 3.3 Interrupt Dispatch in `pdp8_api_step()`

Add interrupt service routine dispatch **after** `apply_skip()` and **before** tick handlers:

```c
int pdp8_api_step(pdp8_t *cpu) {
    if (!cpu || cpu->halted || cpu->memory_words == 0) {
        return 0;
    }

    uint16_t instruction = cpu->memory[cpu->pc];
    cpu->pc = normalise_address(cpu, cpu->pc + 1u);

    /* ... execute instruction opcodes ... */
    
    apply_skip(cpu);
    
    /* === NEW: Interrupt service routine dispatch === */
    if (cpu->interrupt_enable && cpu->interrupt_pending > 0) {
        
        /* Save interrupted context to fixed save area */
        cpu->memory[0x0006] = cpu->ac;        /* Accumulator */
        cpu->memory[0x0007] = cpu->pc;        /* Program Counter */
        cpu->memory[0x0008] = (uint16_t)cpu->link;  /* Link bit */
        
        /* Decrement pending count (one interrupt being serviced) */
        cpu->interrupt_pending--;
        
        /* Disable interrupts to prevent nested service */
        cpu->interrupt_enable = false;
        
        /* Jump to interrupt service routine
         * Standard trap vector location: 0x0010
         * ISR will use skip chain (ISK instructions) to identify
         * which device(s) requested the interrupt.
         */
        cpu->pc = normalise_address(cpu, 0x0010);
    }
    
    /* call registered tick handlers ... */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        /* ... existing tick handler code ... */
    }
    
    return 1;
}
```

---

## 4. Watchdog Device Interrupt Support

### 4.1 Extended Watchdog Commands
In `src/emulator/watchdog.h`, add new command codes:

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

### 4.2 Watchdog Expiry Handler
In `src/emulator/watchdog.c`, modify expiry logic:

```c
static void watchdog_check_expiry(pdp8_t *cpu, pdp8_watchdog_t *wd) {
    if (!wd->enabled || wd->remaining_ds < 0) {
        return;
    }
    
    if (wd->remaining_ds == 0 && !wd->expired) {
        wd->expired = 1;
        
        switch (wd->cmd) {
        case WD_CMD_HALT_ONE_SHOT:
        case WD_CMD_HALT_PERIODIC:
            pdp8_api_set_halt(cpu);
            if (wd->cmd == WD_CMD_HALT_ONE_SHOT) {
                wd->enabled = 0;
            } else {
                wd->remaining_ds = wd->configured_count; /* restart */
            }
            break;
            
        case WD_CMD_RESET_ONE_SHOT:
        case WD_CMD_RESET_PERIODIC:
            pdp8_api_reset(cpu);
            if (wd->cmd == WD_CMD_RESET_PERIODIC) {
                wd->remaining_ds = wd->configured_count; /* restart */
            }
            break;
            
        case WD_CMD_INTERRUPT_ONE_SHOT:
        case WD_CMD_INTERRUPT_PERIODIC:
            pdp8_api_request_interrupt(cpu, PDP8_WATCHDOG_DEVICE_CODE, 0);
            if (wd->cmd == WD_CMD_INTERRUPT_ONE_SHOT) {
                wd->enabled = 0;
            } else {
                wd->remaining_ds = wd->configured_count; /* restart */
            }
            break;
            
        default:
            break;
        }
    }
}
```

---

## 5. Monitor Configuration

### 5.1 Configuration File Support
In `src/monitor_config.h`, already has:

```c
char *watchdog_mode; /* "halt", "reset", "interrupt" */
```

### 5.2 Parser Enhancement
In `src/monitor_platform_posix.c`, extend watchdog mode parsing:

```c
} else if (strcmp(key, "mode") == 0) {
    if (strcmp(value, "halt") == 0) {
        config->watchdog_mode = strdup("halt");
    } else if (strcmp(value, "reset") == 0) {
        config->watchdog_mode = strdup("reset");
    } else if (strcmp(value, "interrupt") == 0) {
        config->watchdog_mode = strdup("interrupt");
    } else {
        /* Unknown mode, use default */
        config->watchdog_mode = strdup("halt");
    }
}
```

### 5.3 Monitor Initialization
In `src/monitor.c` (or equivalent), when setting up watchdog:

```c
if (strcmp(config->watchdog_mode, "interrupt") == 0) {
    /* Watchdog will request interrupts instead of halting/resetting */
    /* This is handled in watchdog.c's expiry check */
}
```

---

## 6. Testing Strategy

### 6.1 Unit Tests (C)
In `tests/test_emulator.c`, add:

```c
/* Test ION/IOFF instruction execution */
static int test_ion_ioff(void)

/* Test interrupt enable/disable state tracking */
static int test_interrupt_enable_flag(void)

/* Test interrupt request queueing */
static int test_interrupt_request_queue(void)

/* Test interrupt service routine dispatch */
static int test_interrupt_dispatch(void)

/* Test multiple interrupts in queue */
static int test_interrupt_multiple_queue(void)

/* Test watchdog interrupt mode */
static int test_watchdog_interrupt(void)
```

### 6.2 Integration Tests (Python)
In `factory/test_watchdog.py`, add:

```python
def test_interrupt_mode_one_shot(lib: ctypes.CDLL) -> None:
    """Test watchdog interrupt mode (one-shot)"""

def test_interrupt_mode_periodic(lib: ctypes.CDLL) -> None:
    """Test watchdog interrupt mode (periodic)"""

def test_ion_ioff_instruction(lib: ctypes.CDLL) -> None:
    """Test ION/IOFF operate instructions"""
```

### 6.3 Assembly Test Programs
Create `demo/interrupt-test.asm`:

```asm
/ Simple interrupt handler test
/ Requests interrupt, enables interrupts, catches it

        * 0000
        JMP START       / Reset vector

        * 0010
        INTR_HANDLER,
        CLA CLL
        / ... interrupt handler code ...
        JMP I 0000      / return from interrupt

START,
        CLA CLL
        ION             / Enable interrupts
        / ... code that causes interrupt ...
        HLT
```

---

## 7. Backward Compatibility

### 7.1 Default Behavior
- `interrupt_enable` defaults to **false** (interrupts disabled on reset)
- Interrupt queue starts empty
- Existing code without ION/IOFF continues to work unchanged
- Watchdog default mode remains "halt" (no change to existing behavior)

### 7.2 API Stability
- All new functions use `pdp8_api_*` prefix (existing convention)
- Existing API functions unchanged
- No changes to `struct pdp8` internals are exposed via public API

---

## 8. Implementation Phases

### Phase 1: Core Interrupt Mechanics (FOUNDATION)
- [ ] Add interrupt state fields to `struct pdp8`
- [ ] Implement ION/IOFF instruction decoding
- [ ] Implement interrupt request queue management
- [ ] Implement interrupt dispatch in `pdp8_api_step()`
- [ ] Add interrupt API functions to `pdp8.h`
- [ ] Reset initialization and cleanup

### Phase 2: Watchdog Integration (DEVICES)
- [ ] Add watchdog interrupt commands (5, 6)
- [ ] Implement watchdog expiry interrupt dispatch
- [ ] Update watchdog register format documentation

### Phase 3: Monitor Support (SYSTEM INTEGRATION)
- [ ] Add monitor config parsing for interrupt modes
- [ ] Wire up watchdog interrupt mode in monitor initialization
- [ ] Test via monitor tool

### Phase 4: Testing & Documentation (VALIDATION)
- [ ] Implement C unit tests
- [ ] Implement Python integration tests
- [ ] Create interrupt example assembly programs
- [ ] Write user guide and API documentation

---

## 9. Known Limitations & Future Work

### Current Design Limitations
1. **Fixed save area** (0x0006-0x0008): Assumes single interrupt context. Nested interrupts require software stack management (ISR can manually save to memory before re-enabling ION).
2. **Single interrupt line**: No per-device vector tables. All devices vector to same ISR (0x0010), which polls via skip chain.
3. **No interrupt masking**: Cannot selectively disable interrupts from specific devices. IOFF disables all interrupts equally.

### Current Design Advantages
- **Matches real PDP-8**: Single hardware interrupt line with software-based device polling
- **Simple**: Counter-based rather than queue-based (less memory, faster)
- **Predictable**: No queue overflow concerns
- **Authentic**: ISR must use skip chain (ISK) to identify devices, exactly like real hardware

### Future Enhancements
- Multi-level interrupt priority (real PDP-8 had 4 levels; would require priority queue)
- Per-device interrupt vector tables (would require storing device_code in interrupt_pending)
- Interrupt masking by device code (would require per-device enable/disable flags)
- Interrupt statistics/monitoring (useful for debugging)

---

## References

### PDP-8 Architecture
- **ION/IOFF**: Group 2 operate instructions for interrupt control
- **Trap vectors**: Standard addresses at 0010-0077 for interrupt service routines
- **Context save**: AC, PC, LINK typically saved by hardware or ISR prologue

### Related Files
- `src/emulator/pdp8.h` - Public API
- `src/emulator/main.c` - CPU core implementation
- `src/emulator/watchdog.c` - Watchdog device
- `tests/test_emulator.c` - Unit tests
- `factory/test_watchdog.py` - Integration tests

