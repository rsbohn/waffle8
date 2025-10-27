# ASCII Paper Tape Format

In addition to the SIXBIT format documented in `hardware-667x-papertape.md`, the waffle8 emulator supports ASCII-encoded paper tapes where each 12-bit word contains a single 8-bit ASCII character.

## ASCII Tape File Format

- Each line begins with a label (2-3 letters) followed by a three-digit octal line number: `LBLNNN:`
- The remainder of the line contains up to 64 octal-encoded ASCII character values (000-377)
- Each octal value represents one ASCII character
- Line feeds are encoded as `012` (octal for ASCII 10)

Example (`tapes/8asm-man.tape`):
```
MAN001: 070 101 123 115 050 061 051 011 011 011 120 104 120 055 070 ...
MAN002: 012
MAN003: 116 101 115 105 012
```

This translates to:
```
8ASM(1)      PDP-8 ...
(blank line)
NAME
```

## Comparison with SIXBIT Format

| Feature | SIXBIT Format | ASCII Format |
|---------|---------------|--------------|
| Encoding | 6-bit characters | 8-bit ASCII |
| Words per char | 2 chars per 12-bit word | 1 char per word |
| Octal range | 000-077 (per halfword) | 000-377 (full byte) |
| Character set | Limited (A-Z, 0-9, punctuation) | Full ASCII (0-127) |
| Use case | Compact PDP-8 text | Modern text files |

## Creating ASCII Tapes

Use `tools/man_to_tape.py` to convert plain text to ASCII tape format:

```bash
# Convert man page to tape
man -P cat ./man/1/8asm.1 | col -b > /tmp/8asm-man.txt
python3 tools/man_to_tape.py /tmp/8asm-man.txt tapes/8asm-man.tape MAN
```

The tool arguments are:
1. Input text file
2. Output tape file (default: `tapes/<input_stem>.tape`)
3. Tape label prefix (default: "MAN")

## Reading ASCII Tapes

Use `demo/ascii-print.asm` to read ASCII tapes and print to the line printer:

```asm
/ Simple ASCII tape reader
IOT 6672    / Select block
IOT 6671    / Skip if ready
IOT 6674    / Read word
AND 0377    / Mask to 8 bits
/ ... print character ...
```

The key difference from SIXBIT reading is:
- No character pair extraction needed
- Mask to 8 bits (`AND 0377`) instead of 6 bits
- Direct ASCII-to-printer output without conversion table

## Files

- `tools/man_to_tape.py` - Text to ASCII tape converter
- `demo/ascii-print.asm` - ASCII tape printer program
- `tapes/8asm-man.tape` - Example: 8asm man page on tape
