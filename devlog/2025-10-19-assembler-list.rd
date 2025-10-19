# PDP-8 Assembler LIST Mode – 2025-10-19

## Objective
Add a LIST mode to `tools/pdp8_asm.py` so that assembling a source file can also produce a human-readable listing (address, emitted word, source text) on STDOUT.

## CLI Shape
- Extend the CLI to accept either `--list` or a subcommand, e.g. `pdp8_asm.py --list <source>`.
- When `--list` is present without an explicit output path, default the S-record output to `<source>.srec` unless `--output` overrides it.
- Support `--list-only` to print the listing without writing S-records (useful for quick inspections).

## Listing Format
- One line per generated word: `AAAA  WWWW  SYMBOL/OPCODE  ; original source` where `AAAA` is the octal address, `WWWW` is the octal word, and `SYMBOL/OPCODE` is the resolved mnemonic or literal.
- For statements that expand into multiple words (e.g., multiple semicolon-separated ops), repeat the source text on each line to ease correlation.
- Include a header with source filename, assembly timestamp, and origin(s). Footer should report totals (number of words, errors encountered).

## Implementation Notes
- Reuse the existing `self.statements` captured during the first pass; iterate them in address order while printing after the second pass resolves symbols.
- To preserve case in the listing, retain the original text from `Statement.text` and the raw input line (before comment stripping) in case we want to echo comments.
- Collect any `AsmError` instances and print them inline before exiting with non-zero status when in LIST mode.
- Ensure listing uses 4-digit octal addresses and words (`{addr:04o}`) to match PDP-8 conventions.

## Testing Ideas
- Add a regression test that assembles a known fixture (e.g., `demo/core.asm`) and checks a captured listing for expected lines.
- Verify that `--list-only` suppresses S-record emission by mocking the filesystem layer or writing to a temporary directory.
- Confirm that errors still print to STDERR and the partial listing (if any) is clearly marked as incomplete.

## Stretch Goals
- Add an option to include symbol table output sorted alphabetically.
- Allow piping the listing into `less` by streaming rather than buffering the entire output.
