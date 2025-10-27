# SIXBIT Encoding Support in Visual Tape Reader

**Date:** 2025-10-27

## Summary

Added SIXBIT character encoding support to the visual tape reader (`demo/scripts/visual_tape_reader.py`), enabling proper display of PDP-8 tapes that use 6-bit character encoding with two characters packed per 12-bit word.

## Background

The PDP-8 commonly used SIXBIT encoding, a 6-bit character set that could pack two characters into each 12-bit word. This was more space-efficient than 8-bit ASCII, though limited to uppercase letters, digits, and a subset of symbols.

The `tapes/lorem.tape` file contains SIXBIT-encoded text stored as binary bit-strings. When parsed into 12-bit words, each word contains:
- High 6 bits: first character (octal 000-077)
- Low 6 bits: second character (octal 000-077)

## Implementation

### SIXBIT Character Map

Added a complete SIXBIT-to-ASCII translation table:
- `000` = space
- `001-032` = A-Z
- `033-037` = special chars `[ \ ] ^ _`
- `040-051` = 0-9
- `052-077` = punctuation `: ; < = > ? @ ! " # $ % & ' ( ) * + , - . /`

### Parser Functions

**`parse_sixbit_octal()`** - New parser for space-separated octal tokens in the 000-077 range. Packs pairs of 6-bit values into 12-bit words:
```python
word = (tokens[i] << 6) | tokens[i+1]
```

**Detection priority** - Updated `detect_and_parse_payload()` to try parsers in order:
1. SIXBIT-octal (most restrictive, 000-077)
2. ASCII-octal (000-377)
3. Bit-strings (least restrictive, 0/1 only)

### Display Updates

Modified all display paths to decode SIXBIT when appropriate:
- `show_block()` - Shows two-character pairs (e.g., `'TH'`)
- `main()` - Added `--sixbit` flag to force SIXBIT interpretation
- `curses_viewer()` - Updated word display, teleprinter output, and header info
- Manual and autoplay modes both decode SIXBIT correctly

### Command-Line Interface

New option:
```bash
--sixbit    interpret words as SIXBIT encoding
```

This flag forces SIXBIT interpretation even for tapes parsed as bit-strings, which is useful since `lorem.tape` stores SIXBIT data in binary format.

## Testing

### Lorem Ipsum Tape

```bash
$ python3 demo/scripts/visual_tape_reader.py tapes/lorem.tape --sixbit --block 1
AU001: 32 words (parser=sixbit)
000: 2410 'TH'
001: 0500 'E '
002: 0317 'CO'
003: 2226 'RV'
004: 0524 'ET'
005: 2405 'TE'
006: 0017 ' O'
007: 2024 'PT'
008: 1115 'IM'
009: 1132 'IZ'
010: 0523 'ES'
011: 0023 ' S'
012: 2001 'PA'
013: 2213 'RK'
...
```

The decoded text reads: "THE CORVETTE OPTIMIZES SPARK PLUG BATTERY CUTTING<EDGE_SLEEK CHASSIS EXHAUST CRANKSHAFT..." (automotive-themed Lorem Ipsum).

### Verification

All 26 blocks in `lorem.tape` decode successfully:
```bash
$ python3 demo/scripts/visual_tape_reader.py tapes/lorem.tape --dump
Tape label=AU blocks=26
  AU001: 32 words parser=bits
  AU002: 32 words parser=bits
  ...
```

With `--sixbit`, each 12-bit word properly displays two characters from the SIXBIT character set.

## Technical Notes

### Encoding Details

SIXBIT words in `lorem.tape`:
- Stored as: binary bit-strings (`010100 001000 000101...`)
- Parsed as: 12-bit words (`2410`, `0500`, `0317`...)
- Decoded as: character pairs (`TH`, `E `, `CO`...)

For example, word `2410` (octal) = `010 100 001 000` (binary):
- High 6 bits: `010100` = `024` octal = 'T'
- Low 6 bits: `001000` = `010` octal = 'H'

### Character Set Limitations

SIXBIT is uppercase-only with limited punctuation. In the decoded text:
- Spaces appear as ` ` (code 000)
- Hyphens appear as `-` (code 075)
- Underscores replace some separators
- `<` and `=` are used for special formatting

## Files Modified

- `demo/scripts/visual_tape_reader.py`
  - Added `SIXBIT_TO_ASCII` mapping table
  - Added `parse_sixbit_octal()` function
  - Updated `detect_and_parse_payload()` parser priority
  - Modified `show_block()`, `main()`, and `curses_viewer()` for SIXBIT display
  - Added `--sixbit` command-line flag

## Future Enhancements

Potential improvements:
- Auto-detect SIXBIT content based on character distribution
- Support for packed vs. unpacked SIXBIT formats
- SIXBIT-to-ASCII conversion utilities
- Proper handling of SIXBIT control characters (if any)

## References

- PDP-8 SIXBIT encoding is documented in DEC manuals
- Similar to other 6-bit character sets used on minicomputers
- Packing efficiency: 2 characters per word vs. 1.5 for ASCII (3 chars in 2 words)
