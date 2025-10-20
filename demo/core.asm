/ core.asm
/ zero page:
// 0002: return address storage (apps call JMS 0002)
// 0003: JMP I 0004 (trampoline instruction set up by INIT)
// 0004: dispatch pointer (points to DISPATCH)
// 0005: BIOS selector (opcode)
// 0006: VPTR 
// 0007: COUNTER (counts BIOS calls)
/
/ Assembler note: Use '* address' to set location counter (not ORG)

    * 6700
/ run 6700 2000
CORETEST,
    JMS I INIT_PTR
    CLA CLL
    TAD OP3
    DCA 0005
    TAD S_PTR
    DCA 0006
    JMS 0002
    HLT

INIT_PTR,
    INIT
S_PTR,
    TEST_STRING
OP3,    03


    * 7000
INIT,
    0
    CLA CLL
    TAD JMP_I_INSTR / Load "JMP I 0004" instruction
    DCA 0003        / Store at 0003
    TAD DISPATCH_ADDR / Load DISPATCH address
    DCA 0004        / Store dispatch pointer at 0004
    JMP I INIT

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
    KL8E_PUTS               / 3 - KL8E string output
    CRASH                   / 4 - Unused

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

CHARMASK,       0377
MINUS_ONE,      7777        / -1 in two's complement
KL8E_PUTS,
    0
    CLA CLL
    TAD 0006                / a string pointer
    TAD MINUS_ONE           / subtract 1
    DCA 0010
KPS_LOOP,
    CLA CLL                 / Clear accumulator and link
    TAD I 0010              / get a word (character)
    AND CHARMASK            / AND AC with character mask
    SNA
    JMP I KL8E_PUTS         / we're done
    DCA 0006                / Store character in 0006 for KL8E_PUTCH
    JMS KL8E_PUTCH
    JMP KPS_LOOP

/ String data at 7700
    * 7700
TEST_STRING,
    0110                    / 'H'
    0145                    / 'e'
    0154                    / 'l'
    0154                    / 'l'
    0157                    / 'o'
    0054                    / ','
    0040                    / ' '
    0127                    / 'W'
    0157                    / 'o'
    0162                    / 'r'
    0154                    / 'l'
    0144                    / 'd'
    0041                    / '!'
    015                     / CR
    012                     / LF
    0                       / null terminator