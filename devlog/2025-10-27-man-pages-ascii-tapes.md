# 2025-10-27 — Documentation: Man pages and ASCII tapes

## Summary
Added man page infrastructure and ASCII paper tape format to complement the existing SIXBIT tape system. Created tools and demos to convert text documentation into printable tape format.

## Man pages
- Created `man/1/8asm.1` in troff format documenting the `8asm.py` assembler tool
- Covers synopsis, options, assembly language conventions, exit codes, and usage examples
- Viewable via `man ./man/1/8asm.1`

## ASCII paper tape format
- Introduced a new ASCII-encoded tape format where each 12-bit word holds one 8-bit ASCII character (vs SIXBIT's two 6-bit chars per word)
- Created `tools/man_to_tape.py` to convert plain text files into octal-encoded ASCII tape format
- Converted the 8asm man page to `tapes/8asm-man.tape` (104 lines, ~89 lines of text)
- Documented the format in `docs/ascii-papertape.md` with comparison table, creation instructions, and reading examples

## Tape printer demo
- Built `demo/ascii-print.asm` to read ASCII tapes and print to line printer
- Simpler than SIXBIT reader: masks to 8 bits, no conversion table needed
- Assembled to `demo/ascii-print.srec` for testing

## Format comparison

| Feature | SIXBIT | ASCII |
|---------|--------|-------|
| Chars per word | 2 | 1 |
| Bits per char | 6 | 8 |
| Character set | A-Z, 0-9, limited punctuation | Full ASCII 0-127 |
| Efficiency | 2x more compact | Simpler to process |

## Files added
- `man/1/8asm.1` — troff man page for 8asm.py
- `tools/man_to_tape.py` — text-to-ASCII-tape converter
- `demo/ascii-print.asm` / `demo/ascii-print.srec` — ASCII tape reader
- `docs/ascii-papertape.md` — format specification and usage
- `tapes/8asm-man.tape` — example ASCII tape with man page text

## Tape reader enhancement
- Extended `tools/tape_reader.py` to support both SIXBIT and ASCII tape formats
- Added automatic format detection by examining first valid line
- SIXBIT format: 6-character binary strings (e.g., `010100`)
- ASCII format: 3-digit octal values (e.g., `070`)
- Updated to handle 2 or 3 character tape identifiers (e.g., `TP001:` or `MAN001:`)
- Tool now reports detected format when displaying output
- Verified with both `tapes/lorem.tape` (SIXBIT) and `tapes/8asm-man.tape` (ASCII)

## Next steps
- Test ASCII tape reading through webdp8 UI
- Consider converting other documentation to tape format
- Add ASCII tape support to paper_tape device if not already present
