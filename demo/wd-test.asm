/ Simple watchdog test program
/ Sets up watchdog in HALT mode with 50 decisecond timeout
/ and prints dots while restarting it

        *0200
START,  CLA CLL
        TAD WDCFG       / Load watchdog config (HALT_PERIODIC mode, 50ds)
        IOT 6552        / Write to watchdog
        
LOOP,   CLA
        TAD DOT         / Load '.'
        JMS PUTCH       / Print it
        IOT 6554        / Restart watchdog
        JMP LOOP        / Continue

WDCFG,  3062            / CMD=3 (HALT_PERIODIC) in bits [11:9], COUNT=50 deciseconds in bits [8:0]
DOT,    0056            / '.' character

/ Teleprinter output routine
PUTCH,  0
SPIN,   IOT 6041        / Skip if ready
        JMP SPIN
        CLA
        TAD PUTCH
        IAC             / Get character from stack
        TAD I PUTCH
        IOT 6046        / Print
        JMP I PUTCH
