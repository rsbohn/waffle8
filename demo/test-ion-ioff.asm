/ Test ION and IOFF assembler support
/ 2025-10-30

*0000
    IOFF            / Disable interrupts (should assemble to 0x0F00)
    ION             / Enable interrupts (should assemble to 0x0F01)
    JMP LOOP        / Jump back

*0010
LOOP, TAD VALUE
    ION             / Re-enable interrupts in loop
    JMP LOOP

*0100
VALUE, 0o0042      / Data value
