# PDP-8 Paper Tape Device (`667x`)

The host paper-tape image format and emulator-side device allow PDP-8 code to stream fixed-size blocks from a prepared tape file. The device occupies IOT slot `067` (octal), so its instructions live at the `667x` opcode group.

## Tape File Format

- Each line begins with a two-letter ASCII label followed by a three-digit octal block number: `AABBB:`.
- The remainder of the line contains 64 six-bit halfwords (`0`/`1` characters), optionally separated into groups. Every pair of halfwords forms one 12-bit PDP-8 word, producing 32 words per block.
- All blocks in a file must share the same two-letter label, and block numbers must be unique.
- Example (`tapes/tp_demo.tape`):
  ```
  TP001: 101010 101010 ... (64 copies)
  TP002: 010100 100101 ... (64 copies)
  ```

The parser validates line structure, enforces the 64 halfword limit, and converts each block into an array of 12-bit words.

## Emulator Attachment

```c
#include "paper_tape_device.h"

pdp8_paper_tape_device_t *dev = pdp8_paper_tape_device_create();
pdp8_paper_tape_device_load(dev, "tapes/tp_demo.tape");
pdp8_paper_tape_device_attach(cpu, dev);
```

The loader call replaces any previously mounted tape. `pdp8_paper_tape_device_label(dev)` returns the two-character label for status displays.

## IOT Interface

The PDP-8 interacts with the device through the standard three-bit micro-operations encoded in `667x`:

| Instruction | Bits | Purpose |
|-------------|------|---------|
| `6671` (`SKP`) | `0x1` | Skip on ready. Use this to poll for data availability. |
| `6672` (`SEL`) | `0x2` | Select a block. The current `AC` must contain the desired 12-bit block number. On success the device rewinds its internal pointer and becomes ready if data exists. |
| `6674` (`RD`)  | `0x4` | Read next word. When ready, the device places the next 12-bit word in `AC`, advances its pointer, and clears ready when the block is exhausted. |

Multiple bits can be combined if desired (`SEL+RD` etc.), though typical usage mirrors other peripheral drivers:

1. Load `AC` with block number (`octal`).
2. Execute `6672` to select the block.
3. Loop: execute `6671`; if it skips, execute `6674` to fetch a word and store it to memory. Repeat until `6671` no longer skips (end of block).

Unloaded blocks (missing in the tape image) leave the device not-ready and reads return without modifying `AC`.

## Monitor Integration Notes

The stock monitor can add a tape command by reusing the existing “wait/skip/read/store” scaffolding used for paper tape. Minimal PDP-8 side logic:

```plain
    TAD BLOCKNUM       / AC <- desired block
    IOT 6672           / select block
READ:
    IOT 6671           / skip when ready
    JMP READ
    IOT 6674           / AC <- next word
    DCA I PTR          / store and advance
    JMP READ
```

Where `PTR` points into the load buffer. Once the block finishes, the skip test falls through and the monitor can advance to the next target region or prompt the operator.

## Testing

`tests/test_emulator.c` includes `test_paper_tape_parser` and `test_paper_tape_device`, which exercise parsing, block limits, and IOT flows end-to-end. Run `make -C tests && ./tests/pdp8_tests` after modifying the device or tape format.
