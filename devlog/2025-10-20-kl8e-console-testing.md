# 2025-10-20: KL8E Console Testing and Skip Instruction Debug

## Summary

Created comprehensive test suite for the KL8E console device (`factory/test_console.py`) and discovered/fixed an issue with skip instruction testing methodology. The console implementation itself was correct, but initial test logic was flawed.

## Problem Discovery

While creating console tests modeled after `factory/test_magtape.py`, encountered unexpected behavior where keyboard skip instructions (KSF, IOT 6031) appeared to always return true regardless of character availability. Initial assumption was that the console implementation had a bug.

## Investigation Process

Created debug script to examine skip mechanism directly by checking Program Counter (PC) values before and after instruction execution:

- **No character ready**: PC 100 → 101 (normal increment)
- **Character ready**: PC 100 → 102 (skipped next instruction)

This revealed that the KL8E console skip mechanism **was working correctly** - the issue was with the test methodology.

## Root Cause

The original `test_skip_instruction` function used a flawed approach:
1. Wrote HLT instructions to detect skips
2. Used complex step counting logic
3. Couldn't reliably distinguish between skip/no-skip conditions

## Solution

Rewrote `test_skip_instruction` to use direct PC examination:
```python
def test_skip_instruction(lib: ctypes.CDLL, cpu: int, skip_instruction: int) -> bool:
    """Test if a skip instruction actually skips by checking PC advancement."""
    pc_before = lib.pdp8_api_get_pc(cpu)
    lib.pdp8_api_step(cpu)
    pc_after = lib.pdp8_api_get_pc(cpu)
    
    # If skip occurred: PC should be test_addr + 2
    # If no skip: PC should be test_addr + 1
    return pc_after == (test_addr + 2)
```

## Test Coverage

Created four test modes in `factory/test_console.py`:

### 1. Keyboard Test (`keyboard`)
- Tests IOT 6031 (KSF - Keyboard Skip if Flag)
- Tests IOT 6036 (KRB - Keyboard Read Buffer)
- Validates character queueing, ready detection, and reading
- Tests multiple character sequences

### 2. Teleprinter Test (`teleprinter`)
- Tests IOT 6041 (TSF - Teleprinter Skip if Flag)
- Tests IOT 6046 (TLS - Teleprinter Load and Send)
- Validates output character capture and string transmission

### 3. Echo Test (`echo`)
- Combined keyboard input → teleprinter output
- Simulates typical console I/O patterns
- Tests realistic character flow scenarios

### 4. Interactive Test (`interactive`)
- Manual testing mode (requires user input)
- Real-time echo demonstration

## IOT Instructions Validated

| Instruction | Octal | Function | Test Coverage |
|-------------|-------|----------|---------------|
| KSF | 6031 | Skip if keyboard flag set | ✓ Ready detection |
| KRB | 6036 | Clear flag + read character | ✓ Character input |
| TSF | 6041 | Skip if teleprinter flag set | ✓ Ready detection |
| TLS | 6046 | Clear flag + send character | ✓ Character output |

## Usage Examples

```bash
# Test keyboard input functionality
python3 factory/test_console.py keyboard

# Test teleprinter output functionality  
python3 factory/test_console.py teleprinter

# Test echo (keyboard → teleprinter)
python3 factory/test_console.py echo

# Interactive testing
python3 factory/test_console.py interactive
```

## Technical Insights

1. **Skip Instructions**: PDP-8 skip instructions increment PC by 2 when condition is true (skipping next instruction), or by 1 when false (normal execution).

2. **Console State**: 
   - `keyboard_flag` starts false (no character ready)
   - `teleprinter_flag` starts true (ready for output)
   - Queueing input sets keyboard_flag appropriately

3. **IOT Microcode**: Instructions use 3-bit microcode field:
   - Bit 0: Skip if flag set
   - Bit 1: Clear flag  
   - Bit 2: Read/write operation

## Validation Results

All tests pass successfully:
- ✓ Keyboard ready detection works correctly
- ✓ Character input/output functions properly
- ✓ Skip instructions behave as expected
- ✓ Multiple character sequences handled correctly
- ✓ Echo functionality operates properly

## Files Modified

- **Created**: `factory/test_console.py` - Complete KL8E console test suite
- **Temporary**: Debug script (removed after issue resolution)

## Lessons Learned

1. When testing hardware emulation, always verify the test methodology before assuming implementation bugs
2. Direct examination of CPU state (PC, registers) provides clearer debugging than indirect approaches
3. Skip instructions are fundamental to PDP-8 I/O - proper testing requires understanding their PC behavior
4. The KL8E console implementation correctly follows PDP-8 IOT standards

This testing framework ensures the KL8E console device can reliably run authentic PDP-8 software requiring console I/O operations.