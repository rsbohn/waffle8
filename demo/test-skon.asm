/ Test SKON (Skip if Interrupt ON) assembler support
/ 2025-10-31

*0000
    IOFF            / Disable interrupts
    SKON            / Should NOT skip (ION is off)
    JMP TEST2       / This should execute

*0004
TEST2, ION          / Enable interrupts
    SKON            / Should skip (ION is on)
    JMP FAIL        / This should be skipped

*0010
    JMP LOOP        / Success - we skipped over the FAIL

*0020
FAIL, HLT           / Failure halt

*0030
LOOP, TAD VALUE
    ION             / Re-enable interrupts
    SKON            / Skip if interrupt ON
    JMP LOOP

*0100
VALUE, 0o0042       / Data value
