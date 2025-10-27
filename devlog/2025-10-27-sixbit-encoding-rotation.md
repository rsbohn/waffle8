2025-10-27: SIXBIT handling and encoding rotation

Summary

Improved the visual tape reader to properly handle SIXBIT format detection and added interactive encoding display rotation using the Tab key in curses mode.

What I changed

- `demo/scripts/visual_tape_reader.py`
  - Refactored tape format detection to be consistent across the entire tape rather than per-line.
  - Changed `detect_and_parse_payload()` to `detect_format()` which returns format name ('sixbit', 'ascii-octal', or 'bits') without parsing.
  - Modified `load_tape()` to detect format once from the first non-empty payload and apply it consistently to all blocks.
  - Added `display_encoding` variable in curses viewer to allow user-controlled display mode independent of tape format.
  - Implemented Tab key handler to rotate through three display encodings: bits → ascii → sixbit.
  - Updated word display logic in three places to support all three encoding modes:
    - `render_head_info()` function
    - Word list display
    - Both now show bits as 12-digit binary, ascii as single character, or sixbit as two 6-bit characters.
  - Updated UI header to show both tape format and current display encoding.
  - Added "Tab: encoding" to the help text.

Technical details

The original design parsed each line independently, which could theoretically allow format changes mid-tape. This was inconsistent with how paper tapes actually work - they maintain a single encoding throughout. The new approach:

1. Detects format from the first non-empty block payload
2. Stores `tape_format` at tape level (returned from `load_tape()`)
3. Uses this format for parsing all subsequent blocks
4. Allows user to view the data in any encoding via `display_encoding` (controlled by Tab)

Display encoding modes:
- **bits**: Shows 12-bit binary representation (e.g., `000001011100`)
- **ascii**: Shows 8-bit ASCII character or '.' for non-printable
- **sixbit**: Shows two 6-bit PDP-8 SIXBIT characters

Verification

```bash
# Test syntax and import
python3 -c "import demo.scripts.visual_tape_reader as vtr; print('Import successful')"

# Test with actual tape file (requires curses session)
python3 demo/scripts/visual_tape_reader.py --curses tapes/8asm-man.tape
# Press Tab to cycle: bits → ascii → sixbit → bits
# Observe header shows "display=bits", "display=ascii", or "display=sixbit"
# Observe word display changes format accordingly
```

Bug fix (same session)

Found and fixed a bug where the curses mode teleprinter output ignored the selected `display_encoding`. The autoplay and manual read (right-arrow) code paths were checking `tape_format` instead of `display_encoding` when deciding how to decode words for printing. Changed both locations (lines 389 and 428) to use `display_encoding` so Tab rotation now properly affects both the visual display and the actual tape reading behavior.

SIXBIT EOL fix (same session)

Fixed SIXBIT mode to properly handle end-of-line (EOL) characters. Previously, EOL characters were being displayed incorrectly in the teleprinter output. Now the code correctly handles both PDP-8 SIXBIT EOL encodings:

1. **Single character EOL**: `0o54` ('<') outputs as `\r\n`
2. **Two character EOL**: `0o36 0o37` ('^_') outputs as `\r\n`
   - `0o36` ('^') = carriage return
   - `0o37` ('_') = line feed
   - Together they form a complete newline

Different tapes use different conventions:
- `lorem.tape` uses the single-character EOL (0o54)
- `1997-02.tape` uses the two-character sequence (^_)

Refactoring (same session)

Extracted duplicated word decoding logic into helper functions for easier maintenance:

- `demo/scripts/visual_tape_reader.py`
  - Added `decode_sixbit_char(code: int) -> str` - Decodes a single 6-bit SIXBIT character with special handling for control codes (0o54, 0o36, 0o37)
  - Added `decode_word_for_teleprinter(word: int, encoding: str) -> str` - Consolidates all 12-bit word decoding logic for teleprinter output
  - Special handling in `decode_word_for_teleprinter` to detect the two-character `^_` sequence (word `0o3637`) and output it as a single `\r\n`
  - Replaced 52 lines of duplicated decoding logic (26 lines × 2 locations) with simple function calls
  - Both autoplay and manual read paths now call the same helper function, ensuring consistent behavior

Verification:
- `lorem.tape` word `0o0754` → `'G\r\n'` (second char is EOL)
- `1997-02.tape` word `0o3637` → `'\r\n'` (entire word is CR+LF)

Next steps

- Consider adding format indicators in the word list (e.g., different color per encoding)
- Add keyboard shortcut help screen (press '?' for help)
- Consider persisting display encoding preference across sessions
