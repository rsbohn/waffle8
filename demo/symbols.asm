/ demo/symbols.asm
// Test using symbolic definitions for IOT constants
// This demonstrates how the assembler resolves symbols used as IOT operands.

        *0200
START,  CLA CLL
        TAD WD_CTRL         / load control word
        IOT WD_WRITE        / attempt to use symbolic IOT constant
        IOT 06551           / explicit literal for comparison
        HLT

// Data / symbolic definitions
WD_CTRL, 03005            / control: CMD=3 HALT one-shot, COUNT small

// Define symbolic IOT constants as data words (consumes memory)
WD_WRITE,   06551
WD_RESTART, 06554
KL8E_READY, 06041
KL8E_XMIT,  06046

        $
