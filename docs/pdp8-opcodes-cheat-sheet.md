# PDP-8 Opcode Cheat Sheet

## Instruction Format
All PDP-8 instructions are 12 bits (octal 0000-7777):

```
 11  9 8 7 6 5 4 3 2 1 0
+-----+-------------------+
| op  |     operand       |
+-----+-------------------+
```

## Memory Reference Instructions (opcodes 0-5)

| Octal | Mnemonic | Name                    | Description                           |
|-------|----------|-------------------------|---------------------------------------|
| 0xxx  | AND      | AND                     | AC ← AC ∧ M[EA]                      |
| 1xxx  | TAD      | Two's complement Add    | AC ← AC + M[EA], carry to Link        |
| 2xxx  | ISZ      | Increment and Skip      | M[EA] ← M[EA] + 1; skip if M[EA] = 0  |
| 3xxx  | DCA      | Deposit and Clear AC    | M[EA] ← AC; AC ← 0                    |
| 4xxx  | JMS      | Jump to Subroutine      | M[EA] ← PC; PC ← EA + 1               |
| 5xxx  | JMP      | Jump                    | PC ← EA                               |

### Address Modes
- **Direct**: EA = address field (0-127 or 0-4095 depending on page bit)
- **Indirect**: EA = M[address field] (bit 8 = 1)
- **Current Page**: address within current page (bit 7 = 0)
- **Page Zero**: address in page 0 (bit 7 = 1)

**Address Format:**
```
 8 7 6    0
+-+------+
|I|P|addr|
+-+------+
```
- I (bit 8): Indirect addressing (1) or direct (0)
- P (bit 7): Page zero (1) or current page (0)
- addr: 7-bit address offset

## Input/Output Transfer (IOT) Instructions (opcode 6)

**Format:** `6dev` where `dev` is device code (octal 000-777)

```
 11    6 5    3 2 1 0
+-------+------+-----+
| 110   | dev  | ops |
+-------+------+-----+
```

### Common IOT Instructions

#### KL8E Console (Keyboard/Teleprinter)
| Octal | Mnemonic | Description                                |
|-------|----------|--------------------------------------------|
| 6031  | KSF      | Keyboard Skip if Flag set                  |
| 6032  | KCC      | Keyboard Clear flag                        |
| 6034  | KRS      | Keyboard Read Static (read without clear) |
| 6036  | KRB      | Keyboard Read Buffer (read and clear)     |
| 6041  | TSF      | Teleprinter Skip if Flag set              |
| 6042  | TCF      | Teleprinter Clear Flag                     |
| 6044  | TPC      | Teleprinter Print Character (no clear)    |
| 6046  | TLS      | Teleprinter Load and Start (print & clear)|

#### Line Printer
| Octal | Mnemonic | Description                                |
|-------|----------|--------------------------------------------|
| 6601  | LSF      | Line printer Skip if Flag set             |
| 6602  | LCF      | Line printer Clear Flag                    |
| 6604  | LPC      | Line printer Print Character              |
| 6606  | LLS      | Line printer Load and Start               |

#### Paper Tape Reader
| Octal | Mnemonic | Description                                |
|-------|----------|--------------------------------------------|
| 6671  | RSF      | Reader Skip if Flag set                   |
| 6672  | RCF      | Reader Clear Flag                         |
| 6674  | RRB      | Reader Read Buffer                        |

## Operate Instructions (opcode 7)

### Group 1 Operate (bit 8 = 0): Operate on AC and Link

**Format:** `7abc` where bits are combined:

| Bit | Octal | Mnemonic | Description                    |
|-----|-------|----------|--------------------------------|
| 7   | +200  | CLA      | Clear AC                       |
| 6   | +100  | CLL      | Clear Link                     |
| 5   | +040  | CMA      | Complement AC                  |
| 4   | +020  | CML      | Complement Link                |
| 3   | +010  | RAR      | Rotate AC and Link Right       |
| 2   | +004  | RAL      | Rotate AC and Link Left        |
| 1   | +002  | RTR      | Rotate AC and Link Right Twice |
| 0   | +001  | IAC      | Increment AC                   |

**Common Combinations:**
| Octal | Combination | Description                          |
|-------|-------------|--------------------------------------|
| 7000  | NOP         | No Operation                         |
| 7001  | IAC         | Increment AC                         |
| 7002  | BSW         | Byte Swap (when no rotate bits set) |
| 7012  | RTR         | Rotate Right Twice                   |
| 7014  | RAL RTR     | Rotate Left Twice                    |
| 7200  | CLA         | Clear AC                             |
| 7300  | CLA CLL     | Clear AC and Link                    |

### Group 2 Operate (bit 8 = 1): Skip Tests

**Format:** `74bc` where:

| Bit | Octal | Mnemonic | Description                    |
|-----|-------|----------|--------------------------------|
| 7   | +200  | CLA      | Clear AC                       |
| 6   | +100  | SMA      | Skip if AC negative (bit 0 set)|
| 5   | +040  | SZA      | Skip if AC Zero                |
| 4   | +020  | SNL      | Skip if Link Non-zero          |
| 3   | +010  | —        | Invert skip sense              |
| 2   | +004  | OSR      | OR Switch Register with AC     |
| 1   | +002  | HLT      | Halt                           |

**Common Skip Instructions:**
| Octal | Mnemonic | Description                          |
|-------|----------|--------------------------------------|
| 7400  | —        | No operation                         |
| 7402  | HLT      | Halt                                 |
| 7404  | OSR      | OR Switch Register                   |
| 7410  | SKP      | Skip unconditionally                 |
| 7440  | SZA      | Skip if AC Zero                      |
| 7450  | SNA      | Skip if AC Non-zero                  |
| 7500  | SMA      | Skip if AC negative                  |
| 7510  | SPA      | Skip if AC Positive                  |
| 7520  | SNL      | Skip if Link Non-zero                |
| 7530  | SZL      | Skip if Link Zero                    |

## Pseudo-Instructions (Assembler Directives)

| Directive | Description                                   |
|-----------|-----------------------------------------------|
| `*nnnn`   | Set origin/location counter                   |
| `nnnn`    | Deposit constant                              |
| `/text`   | Comment                                       |
| `$`       | Current location counter                      |

## Addressing Examples

```assembly
        TAD 0100    / Direct, page zero: AC ← AC + M[0100]
        TAD 1100    / Direct, current page: AC ← AC + M[current_page | 0100]
        TAD 4100    / Indirect, page zero: AC ← AC + M[M[0100]]
        TAD 5100    / Indirect, current page: AC ← AC + M[M[current_page | 0100]]
```

## Programming Patterns

### Subroutine Call
```assembly
        JMS SUBR    / Call subroutine
        ...         / Return here
SUBR,   0           / Return address stored here
        ...         / Subroutine code
        JMP I SUBR  / Return via stored address
```

### Character Output Loop
```assembly
LOOP,   TAD I PTR   / Get character
        SZA         / Skip if null terminator
        JMP OUTPUT  / Output character
        HLT         / Done
OUTPUT, JMS PUTCH   / Output character
        ISZ PTR     / Advance pointer
        JMP LOOP    / Continue
```

### Conditional Skip
```assembly
        TAD VALUE   / Load value
        SMA         / Skip if negative
        JMP POSITIVE / Handle positive case
        JMP NEGATIVE / Handle negative case
```

## Number Formats

- **Octal**: Primary base (0-7777)
- **Decimal**: For constants (0-4095)
- **ASCII**: Character constants use leading quote: `'A` = 0101 octal
- **Negative**: Two's complement within 12 bits

---
*This cheat sheet covers the complete PDP-8 instruction set as implemented in the waffle8 emulator.*