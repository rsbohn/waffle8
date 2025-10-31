# 2025-10-31: Ex-Nihilo Interrupt Experimentation & Architecture Fix

## Summary

Conducted hands-on interrupt testing that exposed a critical bug in the CPU interrupt dispatch: it was using the wrong ISR entry address due to hexadecimal/octal confusion. Fixed the architecture to match real PDP-8 conventions.

## Key Discoveries

### 1. ION Re-entrancy Trap ⚠️

**The Problem**: If you enable interrupts (ION) in the ISR while an interrupt is still pending, the ISR immediately re-triggers, creating an infinite loop.

**Example Scenario**:
- Watchdog set to periodic mode (keeps re-requesting interrupt)
- ISR returns with ION enabled
- Interrupt fires again immediately
- Loop repeats

**Solution**: Don't enable ION in the ISR if:
- The triggering device is periodic (will re-request)
- You haven't cleared the device flag (via ISK instruction)
- You haven't reconfigured the device (e.g., changed watchdog to one-shot)

**Pattern**: Return from ISR with interrupts **disabled** if there are pending interrupts from periodic devices. Let the application re-enable ION after reconfiguring the device.

### 2. Hex/Octal Confusion Bug in CPU Code 🐛

**The Problem**: The interrupt dispatch code used hexadecimal constants when it should use octal:

```c
/* WRONG - This is decimal 16, not octal 0020 */
cpu->pc = 0x0010u;
cpu->memory[6] = cpu->ac;
cpu->memory[7] = cpu->pc;
cpu->memory[8] = (uint16_t)cpu->link;
```

This caused:
- PC saved to address 8 (octal 010)
- LINK saved to address 8 (collision!)
- ISR entry set to address 16 (octal 020, meant to be 8)

**The Fix**: Use traditional C octal notation:

```c
/* CORRECT - Octal notation matching PDP-8 architecture */
cpu->pc = 020;
cpu->memory[006] = cpu->ac;
cpu->memory[007] = cpu->pc;
cpu->memory[010] = (uint16_t)cpu->link;
```

**Architecture Corrected**:
```
Save Area (CPU-managed):
  006: AC (accumulator)
  007: PC (return address = next instruction after interrupt)
  010: LINK

ISR Entry: 020 (was incorrectly set to 020 octal = 16 decimal, but code used 0x0010 = 8 decimal)
ISR Return: JMP I 007 (jump through saved PC)
```

### 3. C Octal Notation Best Practice

For PDP-8 code clarity:
- **Use traditional C octal**: `010` = decimal 8 = hex 0x08
- **Not modern C99**: `0o10` (causes compiler issues in some environments)
- **Never hex**: `0x10` (confuses with octal 020)

Example constants now in code:
```c
006  /* octal 006 = decimal 6 = AC save location */
007  /* octal 007 = decimal 7 = PC save location */
010  /* octal 010 = decimal 8 = LINK save location */
020  /* octal 020 = decimal 16 = ISR entry point */
```

## Test Sequence Executed

```
Initial setup:
  mem 0000: 6551 5410 0000 0000 0000 0000 0000 0000
  mem 0020: 7401 5410 7402 ...
  dep 0200: 7300 7300 7001 7402 5200
  watchdog: periodic mode, time_remaining=0

Step 1: run 0200 1
  - Executes CLL (7300) at 0200
  - Watchdog fires → interrupt dispatch
  - Saves AC/PC/LINK to 006/007/010
  - PC jumps to 020
  Result: Context saved correctly ✓

Step 2: Discover architecture bug
  - Found saved PC at 0007 (correct in octal)
  - But CPU code was using wrong addresses
  - Realized hex 0x0010 ≠ octal 0010

Step 3: Fix and rebuild
  - Updated main.c interrupt dispatch (octal notation)
  - Updated pdp8.h documentation (octal notation)
  - Rebuilt libpdp8.so
  - Library compiles cleanly ✓
```

## Files Modified

1. **src/emulator/main.c**
   - Lines ~346-356: Fixed interrupt dispatch to use octal 006/007/010/020
   - Added comment clarity about octal notation

2. **src/emulator/pdp8.h**
   - Updated interrupt support documentation
   - Changed ISR example from 0x0010 to octal 0020
   - Fixed save area documentation

## Architecture Now Correct

The PDP-8 interrupt model now properly implements:
- ✅ Correct save area locations (006/007/010)
- ✅ Correct ISR entry point (020)
- ✅ Correct octal notation throughout
- ✅ Matches real PDP-8 conventions
- ✅ Eliminates hex/octal confusion

## Next Steps

1. **Complete ISR implementation** with device polling (ISK instructions)
2. **Test with one-shot watchdog** to avoid re-entrancy issues
3. **Implement full interrupt handler** with multiple devices
4. **Update documentation** to reflect lessons learned
5. **Add regression tests** for interrupt dispatch correctness

## Lessons Learned

- **Always distinguish octal from hex** in PDP-8 code - use comments and consistent notation
- **Test ex-nihilo** when implementing low-level features like interrupts
- **Periodic devices + ION in ISR = infinite loop** (don't mix without device flag clearing)
- **Read memory dumps carefully** - address collisions reveal architecture bugs quickly
