# PDP-8 Interrupt Control Quick Reference

## ION/IOFF Instructions

### IOT Method (Device 00) - Recommended
```
6000  - IOFF (disable interrupts)
6001  - ION  (enable interrupts)
6002  - SKON (skip next instruction if ION enabled)
```

### Group 2 Method - Alternative
```
7400  - IOFF (disable interrupts)
7401  - ION  (enable interrupts)
```

## Monitor Testing

```bash
# Start monitor
./monitor

# Test ION/IOFF
pdp8> dep 0 6001        # Store ION instruction at address 0
pdp8> run 0 1           # Execute it
pdp8> regs              # Check: ION=on

pdp8> dep 0 6000        # Store IOFF instruction
pdp8> run 0 1           # Execute it
pdp8> regs              # Check: ION=off

# Test SKON (skip if ION)
pdp8> dep 0 6002 5000   # SKON at 0, JMP 0 at 1
pdp8> run 0 2           # Execute both
pdp8> regs              # PC should be 3 (skipped address 2)
```

## Assembly Usage

```assembly
        ION             ;  Enable interrupts (6001)
        
        ; ... critical code ...
        
        IOFF            ;  Disable interrupts (6000)
        
        SKON            ;  Skip if ION enabled (6002)
        JMP DISABLED     ;  Jump if ION was disabled
        
        ; ... ION was enabled code ...
```

## Interrupt Dispatch Memory Layout

```
0006    Save AC          (processor accumulator)
0007    Save PC          (program counter)  
0010    Save LINK        (link/carry flag)

0020    ISR Entry        (interrupt service routine starts here)
```

## ISR Pattern Example

```assembly
*0020   ; ISR Entry - interrupts are now disabled
        SKON            ; Skip if ION (device 00)
        JMP .+2         ; Jump if ION was disabled
        
        ; Poll watchdog (device 55)
        IOT 6551        ; ISK - skip if watchdog expired
        JMP NO_WD       ; Jump if watchdog not expired
        
        ; Watchdog fired - service it
        IOT 6552        ; WRITE - clear watchdog
        
NO_WD:  ION             ; Re-enable interrupts
        JMP I 0007      ; Return to interrupted code
```

## Device Codes

```
00  - Interrupt Control (6000-6002)
03  - Keyboard (KL8E)
04  - Teleprinter (KL8E)
55  - Watchdog Timer (device 055 octal)
60  - Line Printer
67  - Paper Tape
70  - Magnetic Tape
```

## ISK Instruction Pattern

ISK (Interrupt Skip on Keyboard) is the device polling method:

```
Device Code  Function  Assembly  Effect
---          1         ISK       Skip if device flag set
```

Example for watchdog (device 55):
```
6551  - ISK watchdog (6000 + (55<<3) + 1)
```

## Common Interrupt Scenarios

### Simple Watchdog Interrupt

1. Enable interrupts: `ION`
2. Configure watchdog to fire
3. Watchdog timer expires
4. ISR saves context at 0006/0007/0010
5. ISR jumps to 0020 with ION disabled
6. ISR polls with ISK: `IOT 6551`
7. ISR services watchdog: `IOT 6552`
8. ISR re-enables: `ION`
9. ISR returns: `JMP I 0007`
10. Main program resumes

### Multiple Devices

ISR must poll all devices that could generate interrupts:

```assembly
        SKON            ; Check interrupt control
        JMP CHK_WD
        
CHK_WD: IOT 6551        ; Check watchdog
        JMP CHK_KBD
        
CHK_KBD: IOT 3601        ; Check keyboard
        JMP DONE
        
        ; Service handlers here...
        
DONE:   ION             ; Re-enable interrupts
        JMP I 0007      ; Return
```

## Tips & Tricks

1. **Always re-enable ION before returning** - Otherwise interrupts stay disabled
2. **Use ISK for device polling** - Standard PDP-8 method
3. **Keep ISR short** - Minimize time with interrupts disabled
4. **Save critical registers** - ISR may modify AC, use `TAD` patterns
5. **Disable ION for critical sections** - Protect shared data

## References

- INTERRUPT_IMPLEMENTATION_COMPLETE.md - Full implementation details
- INTERRUPT_CONTROL_COMPLETE.md - Device 00 details
- AGENTS.md - Coding style and conventions
- docs/ - Additional architecture guides
