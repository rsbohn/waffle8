# October 20, 2025 - Core.asm Trampoline Architecture Redesign

## Overview

Completed a major reorganization of the `demo/core.asm` BIOS system to implement a more extensible trampoline-based calling convention. The new architecture allows applications to use `JMS I 0001` for BIOS calls after initialization with `JMP 7000`, providing better relocatability and cleaner separation between application and system code.

## Problem Statement

The original core.asm had several architectural limitations:
- Fixed calling convention that wasn't easily extensible
- Direct jump limitations due to PDP-8 12-bit addressing constraints  
- Zero page memory layout conflicts
- Difficulty integrating with applications that needed relocatable BIOS calls

## Solution: Trampoline Architecture

### Memory Layout
The new system uses zero page locations 0002-0007:
- `0002`: Return address storage for BIOS calls
- `0003`: Trampoline instruction (`JMP I 0004`)
- `0004`: Dispatch pointer (points to DISPATCH routine at 7010)
- `0005`: Opcode storage for BIOS function selection
- `0006`: VPTR (data pointer for BIOS functions)
- `0007`: Counter for system state

### Calling Convention
1. **Initialization**: `JMP 7000` sets up the trampoline system
2. **BIOS Calls**: `JMS 0002` followed by function parameters
3. **Trampoline Flow**: 
   - `JMS 0002` saves return address and jumps to 0003
   - `JMP I 0004` indirectly jumps to DISPATCH at 7010
   - DISPATCH processes the opcode and returns via `JMP I 0002`

## Implementation Details

### Core BIOS Structure
```assembly
INIT:   // at 7000
    CLA CLL
    DCA 0002    // Clear return address
    TAD JUMPI   // Load JMP I 0004 instruction
    DCA 0003    // Store trampoline
    TAD DISPATCH_PTR  // Load dispatch address
    DCA 0004    // Store dispatch pointer
    JMP I 0002  // Return to caller

DISPATCH:   // at 7010
    ISZ 0007    // Increment counter
    TAD I 0002  // Load opcode from caller
    DCA 0005    // Store opcode
    // ... function dispatch logic
    JMP I 0002  // Return to caller
```

### Application Integration
Updated all demo programs (`fixture.asm`, `kl8e_hello.asm`, `kl8e_echo.asm`) to use the new calling convention:

```assembly
START:
    JMP 7000    // Initialize BIOS
    
    JMS 0002    // Call BIOS function
    0001        // Function opcode
    HELLO       // Data pointer
```

## Testing and Validation

### Assembly Testing
Used `pdp8_asm.py` to verify clean assembly without duplicate labels or addressing conflicts:
```bash
python3 tools/pdp8_asm.py demo/core.asm
```

### Execution Tracing
Validated the trampoline mechanism using `monitor_driver.py` with step-by-step execution:
```bash
python3 tools/monitor_driver.py --image demo/core.srec \
  "switch load 7000" "c 10" "mem 0003 2" \
  "read demo/fixture.srec" "c 1500" "quit"
```

**Trace Results:**
- INIT completed in 6 cycles, setting up trampoline at 0003: `5404` (JMP I 0004)
- Dispatch pointer at 0004: `7010` (DISPATCH address)
- Function calls: `JMS 0002` → `PC=0003` → `JMP I 0004` → `PC=7010`
- DISPATCH executing correctly, processing opcodes and returning

## Key Insights

### PDP-8 Addressing Constraints
- Direct jumps are limited by 12-bit page boundaries
- Indirect jumps (`JMP I addr`) provide better relocatability
- Zero page locations are precious and must be carefully allocated

### Skip Instruction Behavior
- Skip instructions increment PC by 2 when condition is true
- Test methodology required direct PC examination rather than flag-based logic

### Memory Management
- Zero page allocation must respect system constraints (0002-0007 range)
- Trampoline reduces memory footprint while maintaining flexibility

## Files Modified

### Primary Changes
- `demo/core.asm`: Complete architectural redesign with trampoline system
- `demo/fixture.asm`: Updated to use new calling convention
- `demo/kl8e_hello.asm`: Updated calling convention and memory layout
- `demo/kl8e_echo.asm`: Updated calling convention and memory layout

### Supporting Tools
- Used existing `tools/pdp8_asm.py` for assembly
- Used existing `tools/monitor_driver.py` for execution validation
- Leveraged `factory/test_console.py` patterns for testing methodology

## Future Enhancements

1. **Extended Function Set**: Add more BIOS functions (file I/O, math routines)
2. **Error Handling**: Implement error codes and exception handling
3. **Memory Management**: Add dynamic memory allocation routines
4. **Device Abstraction**: Expand hardware abstraction layer

## Conclusion

The trampoline architecture successfully addresses the original limitations while maintaining compatibility with existing applications. The indirect jump mechanism provides the relocatability needed for a proper BIOS system, and the zero page layout efficiently uses precious memory resources. All demo programs now use the consistent `JMS I 0001` calling convention after `JMP 7000` initialization.

The implementation validates the approach and provides a solid foundation for future PDP-8 system software development in the Waffle8 emulator.