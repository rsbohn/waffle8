/ mt-load.asm -- load from magnetic tape

* 0010
THERE,  0000

* 0020
_PUTS,  PUTS
* 0200
START,
    JMP LOADER
PBUFFER, 06000
PSELECT,    .
    "U";"n";"i";"t";"?"
    00000
P_READY,    .
    "R";"e";"a";"d";"y";"."
    00000
    05555
    05555

LOADER,
    CLL CLA
    TAD PSELECT
    DCA THERE
    JMS I _PUTS
    HLT
CONFIG,
    CLL CLA
    OSR
    IOT 6501
    TAD P_READY
    DCA THERE
    JMS I _PUTS
    HLT
DO_LOAD,
    IOT 6510        / SKIP if ready
    JMP LOAD_DONE
    IOT 6502        / read word
    DCA I THERE
    JMP DO_LOAD
LOAD_DONE,
    HLT

*0600
M1,     07777
PUTS,   0   / PUTS
LOOP,
    CLL CLA
    TAD I THERE
    SZA
    JMP PCHAR
    JMP I PUTS  / PUTS DONE
PCHAR,
    IOT 6046
    JMP LOOP