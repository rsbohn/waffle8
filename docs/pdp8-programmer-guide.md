# PDP-8 Programmer Guide

This guide collects the practical details you need to write, assemble, and
exercise PDP-8 programs on the Waffle8 emulator. It assumes basic familiarity
with assembly programming and highlights the conventions baked into
`demo/core.asm` and the host tooling in this repository.

## Emulator Primer

- **Word size:** 12-bit words, octal notation everywhere (`0oXXXX` implied).
- **Memory map:** 4K words per field; the core ROM occupies addresses `6700–7777`.
- **Wall clock:** Reading memory address `07760` yields the host's minutes-since-midnight
  (local time, masked to 12 bits). Treat it as read-only; writes have no effect on
  the reported value.
- **Auto-increment registers:** Locations `0010–0017` **pre-increment** automatically
  when used indirectly. For string processing, initialize the pointer to 
  `string_address - 1` so the first access increments to the correct start.
- **Instruction set:** See `docs/pdp8-opcodes-cheat-sheet.md` for a condensed
  reference to memory-reference, IOT, and operate instructions.
- **Label addressing quirk:** In PDP-8 assembly, a label defined with storage
  (`BUFFER, 0`) refers to the **contents** at that address when used in `TAD BUFFER`.
  To load the **address itself**, define a constant (`BUF_ADDR, 0200`) and use
  `TAD BUF_ADDR`. This matters when initializing autoincrement pointers or
  computing addresses—see "Address Constants vs. Labels" below.

## Toolchain Overview

### Address Constants vs. Labels

A common gotcha in PDP-8 assembly is the difference between **labels with storage**
and **address constants**. When you define a label with data:

```asm
BUFFER,  0      / Label at address 0200 with initial value 0
```

Using `TAD BUFFER` loads the **contents** of address 0200 (zero in this case),
not the address 0200 itself. To get the address value, define a constant:

```asm
BUF_ADDR,  0200    / Constant equal to 0200
```

Then `TAD BUF_ADDR` loads the value 0200 into the accumulator.

**This matters for:**

1. **Initializing autoincrement pointers** (locations 0010-0017):
   ```asm
   TAD BUF_ADDR    / Load address 0200
   TAD NEG1        / Subtract 1 → 0177
   DCA PTR         / Store in autoincrement location
   ```

2. **Computing table offsets**:
   ```asm
   TAD TABLE_ADDR  / Load base address
   TAD OFFSET      / Add offset
   DCA POINTER     / Use for indirect access
   ```

3. **Passing addresses to subroutines**:
   ```asm
   TAD STR_ADDR    / Load string address
   DCA PARAM       / Pass to routine
   JMS PRINT_STR
   ```

**Rule of thumb:** If you need to work with an address arithmetically or store
it in a pointer, define it as a constant. If you just need to read/write the
data at that location, a label with storage is fine.

**Common mistake pattern:**
```asm
/ WRONG - loads contents (0), not address (0200)
TAD BUFFER
DCA PTR

/ RIGHT - loads address value
TAD BUF_ADDR
DCA PTR
```

The assembler won't warn you—both are syntactically valid—but the first loads
zero while the second loads 0200.

### Build the Emulator Artifacts

Most workflows rely on the shared library and the interactive monitor.

```bash
# Build the shared library for tests and Python bindings
cc -std=c11 -Wall -Wextra -pedantic -fPIC -shared src/emulator/*.c -o libpdp8.so

# Build the monitor (loads ROMs, inspects memory, runs programs)
make monitor
```

For regression runs, build and execute the unit tests:

```bash
make -C tests
./tests/pdp8_tests
```

### Assemble PAL-Style Sources

Use the in-tree assembler to translate PAL-style source files into Motorola
S-records.

```bash
python3 tools/pdp8_asm.py demo/hello.asm demo/hello.srec

# Emit a listing while still writing the image
python3 tools/pdp8_asm.py --list demo/hello.asm demo/hello.srec
```

**Important assembler syntax notes:**
- Use `* address` to set the location counter (not `ORG`)
- Use `SNA` for "Skip on Non-zero Accumulator" (not `SNZ`)
- String data must be specified as octal character codes (`0110` for 'H')

If you omit the second positional argument, the assembler writes
`<source_basename>.srec` next to the source file.

### Run Code Under the Monitor

The monitor understands the BIOS layout described below. A typical interactive
session looks like:

```bash
./monitor demo/core.srec
```

Monitor commands of interest:

- `read <file>` – Load an additional S-record (your program).
- `switch load <addr>` – Set the PC to `<addr>` (defaults to octal).
- `run <addr> <cycles>` – Set PC and run for a fixed number of cycles.
- `t [cycles]` – Single-step while printing register state.
- `mem <addr> [count]` – Dump memory words.

For scripted runs that need to feed multiple commands reliably, prefer
`tools/monitor_driver.py`:

```bash
python3 tools/monitor_driver.py --image demo/core.srec \
  "read demo/fixture.srec" \
  "switch load 0200" \
  "c 500" \
  "regs"
```

The driver waits for the `pdp8>` prompt between commands, avoiding dropped
input when the monitor polls its console.

## Core BIOS Conventions

`demo/core.asm` provides the minimal “BIOS” that applications rely on. During
link/load, the ROM lives at `6700–7777` and exposes a trampoline-based call
interface.

### Zero Page Usage

The BIOS reserves the following page-zero slots:

| Address | Meaning                                           |
|---------|---------------------------------------------------|
| `0002`  | Return slot for BIOS calls (`JMS 0002`)           |
| `0003`  | `JMP I 0004` trampoline instruction               |
| `0004`  | Pointer to `DISPATCH` routine                     |
| `0005`  | BIOS opcode (selects an entry in the dispatch table) |
| `0006`  | VPTR – parameter word (value or pointer per opcode) |
| `0007`  | Call counter (incremented on each BIOS entry)     |

Treat these addresses as read/write scratch owned by the BIOS. Applications
may use the remaining page-zero locations, keeping the 0010–0017
auto-increment semantics in mind.

### Initialization

Call `INIT` once after loading the ROM. The routine lives at address `7000`
and is a standard subroutine: its first word stores the return address, and it
returns via `JMP I 7000`.

```asm
        JMS     7000        / Initialise BIOS trampoline (sets 0003/0004)
```

Programs that ship inside the ROM (like the self-test at `6700`) use an
indirect call (`JMS I INIT_PTR`) purely so the pointer can be patched without
editing code. External programs can call `JMS 7000` directly.

### Making a BIOS Call

Use the trampoline at `0002` once the BIOS is initialised:

1. Write the opcode you want to invoke into `0005`.
2. Write the parameter (value or pointer) into `0006`.
3. Execute `JMS 0002`.

`DISPATCH` increments the counter at `0007`, looks up the handler in the
dispatch table, and returns to your code via `JMP I 0002`.

| Opcode | Handler         | Purpose                                  | Parameter in `0006` |
|--------|-----------------|------------------------------------------|---------------------|
| `0`    | `PRN_PUTCH`     | Line-printer character output            | ASCII code (low 7 bits used) |
| `1`    | `KL8E_GETCH`    | Console keyboard read (blocking)         | Destination address (overwritten with result) |
| `2`    | `KL8E_PUTCH`    | Console teleprinter output               | ASCII code (low 7 bits used) |
| `3`    | `KL8E_PUTS`     | Console string output                    | Address of null-terminated string |

The handlers follow PDP-8 conventions (poll with `skip if flag`, auto-clear
after service). `KL8E_GETCH` writes the received character back into `0006`
for convenience.

**Important:** `KL8E_PUTCH` and `KL8E_PUTS` both use location `0006` to pass
character data to the output hardware. When writing BIOS routines that call
each other, ensure characters are stored in `0006` before calling `KL8E_PUTCH`.

You can call the handlers directly with `JMS` if you control the AC/link
state, but the trampoline keeps opcodes centralised and makes it easy to
extend the BIOS later.

## Example Workflow

The snippet below prints “Hello from PDP-8!” on the console and halts with
`AC=0`.

```asm
/ hello.asm
        *0200
START,  JMS     7000            / Initialise BIOS

        CLA CLL
        TAD     OPCODE_PUTS
        DCA     0005
        TAD     MESSAGE_PTR
        DCA     0006
        JMS     0002            / KL8E string output

        HLT                     / Done

OPCODE_PUTS,   0003
MESSAGE_PTR,   MESSAGE

        *0300                   / String data (auto-increment safe zone optional)
MESSAGE,
        0110                    / H
        0145                    / e
        0154                    / l
        0154                    / l
        0157                    / o
        0040                    / space
        0146                    / f
        0162                    / r
        0157                    / o
        0155                    / m
        0040                    / space
        0120                    / P
        0144                    / d
        0120                    / P
        0055                    / -
        0100                    / @
        0000                    / Null terminator (end of string)
```

Assemble, load, and execute:

```bash
python3 tools/pdp8_asm.py hello.asm demo/hello.srec
python3 tools/monitor_driver.py --image demo/core.srec \
  "read demo/hello.srec" \
  "switch load 0200" \
  "c 200" \
  "quit"
```

The monitor prints the string through the emulated KL8E teleprinter.

## Debugging Tips

- **Stepping:** `t 5` shows five instruction fetch/execute cycles with register
  updates. Useful to inspect the zero page trampoline in motion.
- **Register snapshots:** `regs` dumps `PC`, `AC`, `LINK`, and halt state.
- **Memory inspection:** `mem 0000 20` dumps the zero page; append `#` to
  specify decimal counts (`mem 6700 #16`).
- **Depositing values:** `dep 0200 4200 7777` overwrites memory words (octal
  by default).
- **Counter checks:** Inspect `0007` after a run to see how many BIOS calls
  your program issued.
- **String debugging:** For `KL8E_PUTS` issues, check that string data is at
  the expected address and that auto-increment location `0010` contains the
  correct pointer values during execution.
- **Character output issues:** If seeing wrong characters, verify that `0006`
  contains the expected character code before `KL8E_PUTCH` executes.

Automated monitors (`tools/monitor_driver.py`) help capture transcripts for
tests. For example, the fixture at `demo/fixture.srec` uses the driver to print
diagnostic characters and assert the BIOS call counter.

## Testing Your Programs

1. Assemble your program into `tests/fixtures` (or similar) and load it from a
   regression harness.
2. Extend `tests/test_emulator.c` with targeted scenarios—use `load_srec_into_cpu`
   to bring images into the emulator, then tick the CPU or call into board APIs.
3. Run `make -C tests` before submitting changes, and clean the directory
   afterwards with `make -C tests clean`.

Where practical, script a monitor run and compare the transcript against
expected output (newline-terminated ASCII makes assertions easy).

## Further Reading

- `docs/pdp8-opcodes-cheat-sheet.md` – Instruction summary.
- `docs/IOT-guide.md` – Deeper dive into IOT construction and device codes.
- `docs/magtape-usage.md` – Using the magtape peripheral from monitor sessions.
- `devlog/2025-10-20-core-trampoline-architecture.md` – Design notes on the
  trampoline-based BIOS included in `demo/core.asm`.

These references, combined with the guide above, should provide enough context
to write new PDP-8 programs, load them into the emulator, and validate their
behaviour on the Waffle8 platform.
