/ test-prn.asm
/ Print "ABC-EFG-" repeatedly using the line printer.
/ The watchdog timer will stop execution.

        *0010
CHAR_TEMP,      0
STR_PTR,        0
STRING_ADDR,    0450            / Address of STRING

        *0020
CR,             0015
LF,             0012
PUTCHR_PTR,     PUTCHR
PRINT_CRLF_PTR, PRINT_CRLF
NEG1,           -1

        *0200
START,  CLA CLL
        TAD STRING_ADDR
        TAD NEG1
        DCA STR_PTR

LOOP,   CLA
        TAD I STR_PTR
        SZA
        JMP PRINT_CHAR
        / Restart string
        CLA CLL
        TAD STRING_ADDR
        TAD NEG1
        DCA STR_PTR
        JMP LOOP

PRINT_CHAR,
        JMS I PUTCHR_PTR
        JMP LOOP

        *0300
PUTCHR,
        0
        DCA CHAR_TEMP
SPIN,
        IOT 6041                / Skip if line printer ready
        JMP SPIN
        CLA
        TAD CHAR_TEMP
        IOT 6046                / Clear flag and print character
        JMP I PUTCHR

PRINT_CRLF,
        0
        CLA
        TAD CR
        JMS I PUTCHR_PTR
        CLA
        TAD LF
        JMS I PUTCHR_PTR
        JMP I PRINT_CRLF

        *0400

        *0450
STRING,
        0101    / A
        0102    / B
        0103    / C
        0055    / -
        0105    / E
        0106    / F
        0107    / G
        0055    / -
        0
