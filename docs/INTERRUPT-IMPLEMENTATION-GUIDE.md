# Hardware Interrupt Support — Implementation Guide

**Date**: October 30, 2025  
**Status**: Plan Complete, Ready for Implementation  
**Effort**: 8-10 developer hours  
**Risk**: Low (fully backward compatible)

---

## What's Being Planned?

We're adding **hardware interrupt support** to the PDP-8 emulator so:
- Devices (like watchdog) can trigger interrupts
- Software can enable/disable interrupts with ION/IOFF instructions
- The CPU properly saves context and jumps to interrupt handlers
- Everything still works with existing code (backward compatible)

## The Three Big Pieces

### 1️⃣ State Management
Add interrupt control state to the CPU:
- **Interrupt enable flag** - Tracks if ION/IOFF is on/off
- **Interrupt queue** - Buffers pending interrupt requests
- **Queue pointers** - Manage FIFO queue of 64 entries

### 2️⃣ ION/IOFF Instructions  
Implement PDP-8's interrupt control opcodes:
- **ION** (6001) - Enable interrupts
- **IOFF** (6000) - Disable interrupts
- These are Group 2 Operate instructions (0x0E00 opcode)

### 3️⃣ Interrupt Dispatch
When an interrupt is pending and enabled:
- Save the interrupted program's state (AC, PC, LINK)
- Disable interrupts to prevent recursion
- Jump to the interrupt service routine (at fixed addresses)

## How to Get Started

### Step 1: Read the Documentation
Start here, then move to:
1. `docs/interrupt-quick-reference.md` - 2 min read, API overview
2. `docs/interrupt-flow-diagram.md` - 5 min, visual walkthrough
3. `docs/interrupt-implementation-plan.md` - 15 min, full details

### Step 2: Understand the Current Code
- `src/emulator/main.c` - Where the CPU executes instructions
- `src/emulator/pdp8.h` - Public API (add new functions here)
- `src/emulator/watchdog.c` - Device that will use interrupts

### Step 3: Implement in Order
Follow the todo list in this repo (use `manage_todo_list`):

**Phase 1: Core (2-3 hours)**
- [ ] Add state fields to struct pdp8
- [ ] Implement ION/IOFF instruction decoding
- [ ] Implement interrupt dispatch in pdp8_api_step()
- [ ] Add interrupt request API functions

**Phase 2: Watchdog (1 hour)**
- [ ] Add watchdog interrupt commands
- [ ] Update watchdog expiry handler

**Phase 3: Monitor (1 hour)**
- [ ] Add config parsing for interrupt mode
- [ ] Wire up watchdog initialization

**Phase 4: Testing (2-3 hours)**
- [ ] Write unit tests
- [ ] Write integration tests  
- [ ] Create example assembly program

### Step 4: Verify & Commit
```bash
make -C tests clean
make -C tests
./tests/pdp8_tests

# Python tests
cd factory
python3 test_watchdog.py
```

## Key Design Decisions

### ✅ Why a Simple Queue?
- Easy to understand and implement
- Sufficient for demo/test use
- Can be upgraded to priority queue later

### ✅ Why Fixed Context Save (0x0006-0x0008)?
- Simple and fast (no stack overhead)
- Single interrupt context (nested requires ISR to save)
- Matches real PDP-8 patterns

### ✅ Why Disable on Dispatch?
- Prevents recursive interrupts
- ISR explicitly enables with ION when ready

### ✅ Why Default Disabled?
- Backward compatible
- Existing code unaffected
- Clear opt-in behavior

## Interrupt Flow (Simple Version)

```
┌──────────────┐
│ Device ready │ (e.g., watchdog expired)
└──────┬───────┘
       │ calls pdp8_api_request_interrupt()
       ▼
┌──────────────────┐
│ Interrupt queued │ (buffered, waiting)
└──────┬───────────┘
       │ when software executes ION
       ▼
┌────────────────────────────────┐
│ Interrupt enabled              │
│ (at end of next instruction)   │
└──────┬─────────────────────────┘
       │ pdp8_api_step() dispatch
       ▼
┌──────────────────────────────────┐
│ 1. Save AC, PC, LINK to memory  │
│ 2. Disable interrupts            │
│ 3. Jump to handler at 0x0010     │
└──────┬───────────────────────────┘
       │
       ▼
┌──────────────────────────────────┐
│ Interrupt handler runs           │
│ (ISR code in assembly)           │
└──────┬───────────────────────────┘
       │ executes ION to enable interrupts
       ▼
┌──────────────────────────────────┐
│ Returns to interrupted program   │
│ (ISR restores context)           │
└──────────────────────────────────┘
```

## File Locations & Changes

| Component | File | Changes |
|-----------|------|---------|
| State fields | `src/emulator/main.c` | Add struct fields |
| ION/IOFF | `src/emulator/main.c` | Update `operate_group2()` |
| API functions | `src/emulator/pdp8.h` | Export 3 new functions |
| Dispatch logic | `src/emulator/main.c` | Add to `pdp8_api_step()` |
| Watchdog support | `src/emulator/watchdog.c` | Add commands, update expiry |
| Monitor config | `src/monitor_platform_posix.c` | Parse "interrupt" mode |
| Tests | `tests/test_emulator.c` | Add 5-6 test functions |
| Tests | `factory/test_watchdog.py` | Add 2-3 test functions |
| Examples | `demo/interrupt-test.asm` | Create ISR example |

## API Reference (Quick)

### New Functions to Add
```c
/* Request an interrupt from a device */
int pdp8_api_request_interrupt(pdp8_t *cpu, uint8_t device_code, uint8_t trap_vector);

/* Check if interrupt pending (non-blocking) */
int pdp8_api_peek_interrupt_pending(const pdp8_t *cpu);

/* Clear all pending interrupts */
int pdp8_api_clear_interrupt_queue(pdp8_t *cpu);
```

### New Opcodes to Support
```
ION  (6001 octal) = 0x0E01 hex = Group 2 operate + bit 0
IOFF (6000 octal) = 0x0E00 hex = Group 2 operate (bit 0 clear)
```

## Assembly Example

```asm
/ Simple interrupt handler

        * 0000
        JMP START

        * 0010          ; Trap vector (interrupt handler entry)
INTR,   CLA             ; Clear everything
        TAD 0007        ; Get saved PC (resume address)
        DCA 0011        ; Temp storage
        ION             ; Re-enable interrupts
        JMP I 0011      ; Return to interrupted code

        * 0200
START,  CLA
        ION             ; Enable interrupts
        / ... normal program ...
        HLT
```

## Testing Checklist

Before you call it done:

- [ ] `make -C tests` passes (no regression)
- [ ] All new interrupt tests pass
- [ ] `demo/interrupt-test.asm` assembles and runs
- [ ] Watchdog works in all three modes (halt, reset, interrupt)
- [ ] Old code still works (backward compat)
- [ ] No undefined behavior in valgrind
- [ ] Documentation updated

## Questions? Reference:

| Question | Answer |
|----------|--------|
| Where's the full spec? | `docs/interrupt-implementation-plan.md` |
| What's the API summary? | `docs/interrupt-quick-reference.md` |
| Show me diagrams | `docs/interrupt-flow-diagram.md` |
| What's a quick overview? | `docs/INTERRUPT-PLAN-SUMMARY.md` |
| Need a cheat sheet? | `docs/interrupt-cheat-sheet.md` |

## Success Criteria

✅ You're done when:
- [ ] ION/IOFF instructions execute
- [ ] Interrupts are queued and dispatched
- [ ] Watchdog can trigger interrupts
- [ ] All tests pass
- [ ] No regressions
- [ ] Code follows style guide (from AGENTS.md)
- [ ] Documented with dev log entry

## Key Takeaways

1. **Small changes, big impact**: ~300 lines of code across several files
2. **Backward compatible**: Old code works unchanged
3. **Well-documented**: 5 reference docs to support implementation
4. **Testable**: Unit tests, integration tests, assembly examples
5. **Phased approach**: Can implement in 4 phases, test each one

## Next Actions

1. **Review** - Read the plan documents in this folder
2. **Decide** - Is this approach aligned with project goals?
3. **Start** - Begin Phase 1 (core mechanics in main.c)
4. **Test** - Add unit tests immediately
5. **Integrate** - Move to Phase 2 when Phase 1 is tested

---

**Questions?** Check the docs folder. Not answered? Open an issue or start a discussion.

**Ready to implement?** Start with Phase 1 from the todo list above.

**Need a refresher?** This file summarizes everything. Details are in the 5 reference docs.

Good luck! 🎉
