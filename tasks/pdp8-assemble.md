# PDP-8 Assembly Task

This task assembles PDP-8 assembly language files using the `tools/8asm.py` assembler.

## Task Behavior

When given a `.asm` source file, this task:
1. Assembles the file using `tools/8asm.py`
2. Produces a Motorola S-record output file with `.srec` extension
3. Places the output in the same directory as the source file

## Examples

```bash
# Assemble demo/foo.asm → demo/foo.srec
python3 tools/8asm.py demo/foo.asm demo/foo.srec

# Assemble with listing output
python3 tools/8asm.py demo/foo.asm demo/foo.srec --list
```

## Input/Output Convention

- **Input**: `<path>/<filename>.asm`
- **Output**: `<path>/<filename>.srec`

For example:
- `demo/foo.asm` → `demo/foo.srec`
- `demo/ptascii.asm` → `demo/ptascii.srec`
- `demo/interrupt-test.asm` → `demo/interrupt-test.srec`

## Assembler Features

The assembler (`tools/8asm.py`) supports:
- Memory-reference instructions: AND, TAD, ISZ, DCA, JMS, JMP
- Group 1 operate micro-ops: CLA, CLL, CMA, CML, RAR, RAL, RTR, RTL, BSW, IAC
- Group 2 operate micro-ops: SMA, SZA, SNL, SPA, SNA, SZL, CLA, OSR, HLT
- Interrupt control: ION, IOFF, SKON
- IOT instructions with numeric or mnemonic operands
- Origins via `*<octal>`
- Labels with comma suffix: `LABEL,`
- Octal literals and quoted character constants

## Output Format

The assembler produces Motorola S-record (S1) format with:
- Byte addresses (two bytes per 12-bit PDP-8 word)
- Little-endian encoding (low byte first, high nibble second)
- Optional START record for entry point

## Error Handling

If assembly fails, the assembler will report:
- Line number where error occurred
- Error description
- Source text snippet

Exit code will be non-zero on failure.
