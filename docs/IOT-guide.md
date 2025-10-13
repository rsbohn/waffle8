# PDP-8 IOT Quick Guide

Input/Output Transfer (IOT) instructions give the PDP-8 CPU a uniform way to talk to peripherals. Each IOT combines a six-bit device code with three action bits, so a single instruction can ask a device to do up to three micro-operations.

## Instruction Layout

```
   15         9 8   6 5   0
  +-------------+-----+-----+
  | 111 (IOT)   | dev | ops |
  +-------------+-----+-----+
```

- `dev` (bits 5–3): 6-bit device code (octal `00`–`77`).
- `ops` (bits 2–0): three micro-op bits; a device decides what each bit means.
- Bits 8–6 are unused and must be zero; setting them still addresses the same device.

To form an IOT manually:

```
opcode = 0o6000 | ((device & 0o77) << 3) | (micro_ops & 0o7)
```

## Line Printer Example (device `060`)

The emulator’s line printer exposes three operations:

| Bit | Octal mnemonic | Meaning                                         |
|-----|----------------|-------------------------------------------------|
| 0   | `…001`         | Skip if the printer is ready                    |
| 1   | `…002`         | Clear the ready flag (mark printer busy)        |
| 2   | `…004`         | Print the character in `AC` (low 7 bits used)   |

Combine bits to queue multiple actions in one IOT. For example:

- `0o6601`: skip when ready (`device=060`, ops=`001`).
- `0o6604`: print the character in `AC`.
- `0o6606`: clear ready *and* print.

### Sample Routine

```asm
        / Assume AC holds a 7-bit ASCII character.
PRINT,  IOT     6601            / Skip when printer ready?
        JMP     PRINT           / Loop until ready
        IOT     6606            / Clear flag and emit character
        JMS     NEXTCHAR        / Load next character and repeat...
```

- The first IOT waits until the device sets its ready flag, skipping the `JMP PRINT` when ready.
- `6606` uses bits 1 and 2 together so the printer acknowledges the character and returns to the ready state.

## Tips

- Always mask data to 12 bits before issuing `IOT` instructions (`AC` is only 12 bits wide).
- Many devices assert the ready flag immediately after a service (as the line printer does). Still, polling with `skip if ready` is the safest pattern.
- Reserve device codes consistently across the project to avoid overlapping handlers. The line printer uses `060`; choose an unused code for new peripherals and register its handler via `pdp8_api_register_iot`.
