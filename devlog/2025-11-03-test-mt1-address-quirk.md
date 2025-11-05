# 2025-11-03: test-mt1 Debug Session and PDP-8 Address Constant Quirk

## Problem Statement

The `demo/test-mt1.asm` program was intended to read a 6-word header from magtape unit 1, decode the first 3 words (containing a 6-character sixbit-encoded label), and print the label to stdout via the line printer. Initial testing showed the program was reading from the wrong magtape unit and failing to decode or print the label correctly.

## Investigation Process

Through interactive debugging with the monitor, we traced program execution step-by-step and uncovered three distinct bugs:

### Bug #1: Wrong Magtape Unit Selection

**Problem:** The initialization code used `TAD NEG1` before the `IOT 6701` magtape selection instruction, loading -1 (octal 7777) into the accumulator instead of 1.

**Result:** The magtape controller selected unit 0 instead of unit 1. Verified by examining `show magtape` output showing unit 0 position advancing.

**Fix:** Added constant `UNIT1, 1` and changed to `TAD UNIT1`.

```asm
/ WRONG
TAD NEG1        / Loads 7777 (octal -1)
IOT 6701        / Selects unit based on AC

/ RIGHT
TAD UNIT1       / Loads 0001
IOT 6701        / Selects unit 1
```

### Bug #2: Loading Address Contents Instead of Address Values

**Problem:** The code used `TAD HEADER` and `TAD LABEL_BUF` to initialize autoincrement pointers at locations 0010-0017. In PDP-8 assembly, `TAD HEADER` loads the **contents** of the memory location labeled HEADER, not the address itself.

**Result:** Since HEADER was initialized to zero, the pointers were set to 0 instead of the intended addresses (0050 for HEADER, 0056 for LABEL_BUF). Data was written to wrong memory locations, corrupting the zero page.

**Fix:** Defined address constants `HDR_ADDR, 0050` and `LBL_ADDR, 0056`, then changed all references to use these constants.

```asm
/ WRONG
TAD HEADER      / Loads contents at 0050 (which is 0)
TAD NEG1
DCA HDR_PTR     / Stores 7777 instead of 0047

/ RIGHT  
HDR_ADDR,  0050 / Define constant with address value
...
TAD HDR_ADDR    / Loads the value 0050
TAD NEG1        / Subtracts 1 → 0047
DCA HDR_PTR     / Stores 0047 (will auto-increment to 0050)
```

### Bug #3: Incorrect Indirect Addressing in SIX_TO_ASCII

**Problem:** The SIX_TO_ASCII routine used `TAD I CHARMAP_PTR` to load the base address of the character map table. `CHARMAP_PTR` was defined as a constant containing 0500 (the address of CHARMAP). Using indirect addressing (`TAD I`) loaded the **contents** at address 0500 (which is 0040, the first entry in CHARMAP), not the address 0500 itself.

**Result:** The table lookup computed wrong addresses, returning 0 for all sixbit values. LABEL_BUF remained empty and nothing was printed.

**Fix:** Changed `TAD I CHARMAP_PTR` to `TAD CHARMAP_PTR` to load the constant value directly.

```asm
/ WRONG
TAD I CHARMAP_PTR   / Loads contents at address 0500 (0040)

/ RIGHT
TAD CHARMAP_PTR     / Loads the value 0500 itself
```

## The PDP-8 Address Constant Pattern

This debugging session exposed a fundamental quirk in PDP-8 assembly programming:

**Labels with storage** (defined as `LABEL, value`) refer to the **contents** at that address when used in instructions.

**Address constants** (defined as `CONST, 0200`) provide the **address value** itself.

This distinction matters when:
- Initializing autoincrement pointers (locations 0010-0017)
- Computing table offsets or addresses arithmetically
- Passing addresses to subroutines
- Building indirect addressing chains

The assembler won't warn you—both patterns are syntactically valid—but choosing the wrong one leads to subtle runtime bugs where you're working with 0 instead of the intended address.

## Testing and Verification

After all three fixes:

```
pdp8> magtape 1 rewind
pdp8> run 200 2000
PTCOPY

Executed 343 cycle(s). PC=0250 HALT=yes
```

The program correctly:
1. Selected magtape unit 1
2. Read the 6-word header: `2024 0317 2031 0123 0311 1100`
3. Decoded the first 3 words from sixbit to ASCII: "PTCOPY"
4. Printed the label to stdout via the line printer
5. Halted cleanly

Verification with the magtape inspection tool confirmed the label:

```bash
$ python3 tools/magtape_tool.py inspect magtape/record-20251102-232727.tap --decode
Header:
  Label     : PTCOPY
  Data format: ASCII
```

## Documentation Updates

Added comprehensive coverage of the address constant vs. label quirk to `docs/pdp8-programmer-guide.md`:

1. Brief mention in the "Emulator Primer" section
2. New subsection "Address Constants vs. Labels" with:
   - Core concept explanation
   - Three concrete use cases
   - Code examples showing wrong vs. right patterns
   - Rule of thumb for choosing between them

This should prevent future developers from encountering the same trap.

## Lessons Learned

1. **Interactive debugging is essential** for catching addressing bugs—single-stepping through initialization code and examining memory at each step revealed all three issues.

2. **The PDP-8 has no "address-of" operator** like modern assembly languages. You must explicitly define constants when you need address arithmetic.

3. **Autoincrement pointers are powerful but unforgiving**—initializing them with the wrong value (0 instead of the intended address) silently corrupts memory in hard-to-trace ways.

4. **Consistent naming helps**: Using `_ADDR` suffix for address constants and plain names for storage labels makes the distinction visible in code.

5. **Test with known data**: Having the magtape inspection tool to verify tape contents independently was crucial for confirming the program logic was correct once the addressing bugs were fixed.

## Files Modified

- `demo/test-mt1.asm` - Fixed all three bugs
- `docs/pdp8-programmer-guide.md` - Added address constant documentation
