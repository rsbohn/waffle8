/ core.asm
/ zero page:
// 0002: call vector
// 0003: entry point 7000
// 0004: opcode
// 0005: vptr
// 0006: return addr

    * 7000
DISPATCH,
    CLA CLL
    TAD 0007
    IAC
    DCA 0007
    CLL
    TAD 0004        / get the opcode
    TAD DTABLE_PTR
    DCA OPVECTOR
    TAD I OPVECTOR
    DCA OPVECTOR
    JMS I OPVECTOR
    JMP I 0001      / return from core
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

    * 7030
INIT,
    CLA CLL
    TAD VEC02
    DCA 0002
    TAD VEC03
    DCA 0003
    HLT

VEC02, 5403
VEC03, 7000

PRN_PUTCH,
    0                       / Return address
PRN_WAIT,
    IOT 6601                / Skip when printer ready
    JMP PRN_WAIT            / Wait if not ready
    CLA CLL
    TAD 0005                / Get the vptr as a simple value
    IOT 6606                / Clear ready + print character
    JMP I PRN_PUTCH         / Return

KL8E_GETCH,
    0                       / Return address
KBD_WAIT,
    IOT 6031                / KSF: Skip when keyboard ready
    JMP KBD_WAIT            / Wait if not ready
    IOT 6036                / KRB: Clear flag + read character into AC
    DCA 0005                / Store character in vptr location
    JMP I KL8E_GETCH        / Return

KL8E_PUTCH,
    0                       / Return address
TPR_WAIT,
    IOT 6041                / TSF: Skip when teleprinter ready
    JMP TPR_WAIT            / Wait if not ready
    CLA CLL
    TAD 0005                / Get the vptr as a simple value
    IOT 6046                / TLS: Clear flag + transmit character
    JMP I KL8E_PUTCH        / Return
