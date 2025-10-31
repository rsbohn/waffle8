# Assembler Support for SKON (Skip if Interrupt ON)

**Date**: 2025-10-31  
**Status**: ✅ COMPLETE  
**PR Impact**: Minimal (1 file changed, ~15 lines)

## Summary

Added SKON mnemonic to the PDP-8 assembler (tools/pdp8_asm.py). Programs can now use readable `SKON` mnemonic instead of raw IOT encoding `0o6002` or `IOT 6002`.

## Changes

### File: tools/pdp8_asm.py

#### 1. Added IOT_MNEMONICS Dictionary (lines 79-81)
```python
IOT_MNEMONICS = {
    "SKON": 0o6002,  # Skip if Interrupt ON (device 00, function 2)
}
```

#### 2. Updated Module Docstring (lines 8-9, 11)
```python
* IOT mnemonics: SKON (skip if interrupt on)
...
* Interrupt control: ION (enable), IOFF (disable), SKON (skip if on)
```

#### 3. Added IOT Mnemonic Parsing (lines 213-221)
```python
if op in IOT_MNEMONICS:
    # IOT mnemonic like SKON - assemble as a direct IOT value
    # Format as octal string to preserve the intended value
    self.statements.append(
        Statement("iot", location, (f"0o{IOT_MNEMONICS[op]:o}",), line_no, part, raw.rstrip("\n"))
    )
    location += 1
    continue
```

## Implementation Details

### Why SKON is an IOT Instruction

SKON is part of the Interrupt Control device (device 00):
- **Device Code**: 00 (octal)
- **Function Code**: 2 (octal)
- **Full Encoding**: 6000 (device base) + 002 (function) = **6002 octal**

In contrast, ION/IOFF are Group 2 operate instructions (device independent).

### Distinction Between Two Interrupt Control Methods

The PDP-8 architecture provides **two ways** to control interrupts:

#### Method 1: Group 2 Operate (Part of CPU)
- ION  = 0o7401 (Group 2 operate with bit 0 set)
- IOFF = 0o7400 (Group 2 operate with bit 0 clear)
- Already supported in assembler as mnemonics

#### Method 2: IOT Method (Interrupt Control Device 00)
- IOFF (IOT 6000) = Device 00, function 0
- ION  (IOT 6001) = Device 00, function 1
- SKON (IOT 6002) = Device 00, function 2 (skip if interrupt on)
- SKON is now supported as a mnemonic

Both methods control the same `interrupt_enable` flag in the CPU.

## Usage Examples

### Simple Interrupt Gate
```asm
*0000
    ION             / Enable interrupts
    SKON            / Skip if interrupt already on
    JMP CONTINUE    / Continue if not on
    HLT             / Error - interrupt already enabled

CONTINUE, TAD DATA
    IOFF            / Disable before critical section
    DCA RESULT
    ION             / Re-enable afterward
```

### Polling Pattern with SKON
```asm
*0000
    ION             / Enable interrupt
LOOP, SKON          / Skip if ION enabled (conditional check)
    JMP LOOP        / Loop if not enabled yet
    TAD VALUE
    ISZ COUNTER
    JMP LOOP
```

## Testing

### Verified Assembly

Created demo/test-skon.asm with comprehensive tests:
- ✅ SKON assembly (0o6002)
- ✅ Combined with IOFF/ION
- ✅ Label resolution works correctly

### Test Results

```
✅ SKON: PASS
  Expected: 0o6002 (IOT instruction)
  Got:      0o6002
  
✅ ION:  PASS (0o7401 - Group 2 operate)
✅ IOFF: PASS (0o7400 - Group 2 operate)
```

### Assembler Tests

```
$ python3 tests/test_assembler_listing.py
..
----------------------------------------------------------------------
Ran 2 tests in 0.157s

OK
```

## Backward Compatibility

✅ **Fully backward compatible**
- Existing programs using IOT 6002 still work
- Existing ION/IOFF programs unchanged
- New programs can use SKON mnemonic for clarity
- No breaking changes

## Relationship to Interrupt Support

SKON pairs with existing interrupt infrastructure:

| Feature | Implementation | Status |
|---------|-----------------|--------|
| ION/IOFF decode | CPU (main.c) | ✅ Complete |
| ION/IOFF mnemonics | Assembler (Group 2) | ✅ Complete |
| SKON decode | CPU (interrupt_control.c) | ✅ Complete |
| SKON mnemonic | Assembler (IOT) | ✅ Complete (this PR) |
| ISK device mnemonics | Assembler (Phase 2) | 🔄 Planned |

## Related Code

- **CPU Implementation**: `src/emulator/interrupt_control.c` (SKON handler)
- **CPU Instruction Decode**: `src/emulator/main.c` (Group 2 operate for ION/IOFF)
- **Assembly Example**: `demo/test-skon.asm` (new test program)
- **Related Docs**: `AGENTS.md` → Interrupt Support Conventions

## Files Modified

1. `tools/pdp8_asm.py` - Added SKON mnemonic support
2. `demo/test-skon.asm` - New comprehensive SKON test program
3. `ASSEMBLER_SKON_SUPPORT.md` - This document

## One-Liner Commit Message

> Add SKON assembler mnemonic for interrupt control skip instruction

## Full Commit Description

```
tools/pdp8_asm: Add SKON mnemonic for interrupt control skip

Add SKON (0o6002, interrupt control device 00 function 2) as a
recognized IOT instruction mnemonic. SKON skips the next instruction
if interrupts are enabled (ION status).

This complements existing ION (0o7401) and IOFF (0o7400) Group 2
operate mnemonics, providing all interrupt control instructions as
readable mnemonics rather than raw opcodes.

SKON is implemented via IOT (Input/Output Transfer) encoding and is
distinct from the Group 2 operate ION/IOFF instructions, though both
control the same interrupt_enable flag.

Tested with demo/test-skon.asm and existing test suite.

No breaking changes; existing programs using IOT 6002 still work.
```

---

**Impact**: Minimal | **Risk**: Very Low | **Benefit**: Improved readability for interrupt-driven code
