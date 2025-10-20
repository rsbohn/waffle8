/ core.asm
/ zero page:
// 0002: return address storage (apps call JMS 0002)
// 0003: JMP I 0004 (trampoline instruction set up by INIT)
// 0004: dispatch pointer (points to DISPATCH)
// 0005: BIOS selector (opcode)
// 0006: VPTR 
// 0007: COUNTER (counts BIOS calls)

    * 7000
INIT,
    CLA CLL
    TAD JMP_I_INSTR / Load "JMP I 0004" instruction
    DCA 0003        / Store at 0003
    TAD DISPATCH_ADDR / Load DISPATCH address
    DCA 0004        / Store dispatch pointer at 0004
    HLT

JMP_I_INSTR, 5404   / JMP I 0004 instruction (5400 + 0004)
DISPATCH_ADDR, DISPATCH / Address of dispatch routine

DISPATCH,
    CLA CLL
    TAD 0007
    IAC
    DCA 0007
    CLL
    TAD 0005        / get the opcode from BIOS selector
    TAD DTABLE_PTR
    DCA OPVECTOR
    TAD I OPVECTOR
    DCA OPVECTOR
    JMS I OPVECTOR
    JMP I 0002      / return to caller (0002 has return address)
CRASH,
    0
FINAL,
    HLT

OPVECTOR,
    0
DTABLE_PTR,
    DTABLE
DTABLE,
    PRN_PUTCH               / 0 - Printer output
    KL8E_GETCH              / 1 - KL8E keyboard input
    KL8E_PUTCH              / 2 - KL8E teleprinter output
    CRASH                   / 3 - Unused

PRN_PUTCH,
    0                       / Return address
PRN_WAIT,
    IOT 6601                / Skip when printer ready
    JMP PRN_WAIT            / Wait if not ready
    CLA CLL
    TAD 0006                / Get the vptr as a simple value (updated to 0006)
    IOT 6606                / Clear ready + print character
    JMP I PRN_PUTCH         / Return

KL8E_GETCH,
    0                       / Return address
KBD_WAIT,
    IOT 6031                / KSF: Skip when keyboard ready
    JMP KBD_WAIT            / Wait if not ready
    IOT 6036                / KRB: Clear flag + read character into AC
    DCA 0006                / Store character in vptr location (updated to 0006)
    JMP I KL8E_GETCH        / Return

KL8E_PUTCH,
    0                       / Return address
TPR_WAIT,
    IOT 6041                / TSF: Skip when teleprinter ready
    JMP TPR_WAIT            / Wait if not ready
    CLA CLL
    TAD 0006                / Get the vptr as a simple value (updated to 0006)
    IOT 6046                / TLS: Clear flag + transmit character
    JMP I KL8E_PUTCH        / Return
