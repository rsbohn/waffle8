/ wd-perf.asm
/ Measure emulator throughput using the watchdog tick mode.
/ The watchdog runs in flag-only periodic mode so the program can spin on
/ `IOT 6551` without halting or enabling interrupts.

        *0040
/ ------------------------------------------------------------
/ Zero-page variables and constants
/ ------------------------------------------------------------
STR_PTR,        0
CHBUF,          0
OCT_TMP,        0
COUNT_LO,       0
COUNT_MI,       0
COUNT_HI,       0
CPS_LO,         0
CPS_MI,         0
CPS_HI,         0

HEADER_PTR,     HEADER_STR
ALIGN_PTR,      ALIGN_STR
RAW_PTR,        RAW_STR
SEC_PTR,        SEC_STR
CRLF_PTR,       CRLF_STR

WD_CFG,         07024           / CMD=7 (tick periodic), COUNT=024 (2s)
ASCII_ZERO,     0060
QUOTE_CHAR,     0047
K0007,          0007
K0070,          0070
K0700,          0700
K7000,          7000

/ ------------------------------------------------------------
/ Page-zero helpers (so JMS targets stay in range)
/ ------------------------------------------------------------
        *0100
PRINT_OCTAL,    0
        DCA OCT_TMP

        TAD OCT_TMP
        AND K7000
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        TAD ASCII_ZERO
        JMS PUTCH

        TAD OCT_TMP
        AND K0700
        RAR
        RAR
        RAR
        RAR
        RAR
        RAR
        TAD ASCII_ZERO
        JMS PUTCH

        TAD OCT_TMP
        AND K0070
        RAR
        RAR
        RAR
        TAD ASCII_ZERO
        JMS PUTCH

        TAD OCT_TMP
        AND K0007
        TAD ASCII_ZERO
        JMS PUTCH
        JMP I PRINT_OCTAL

PUTS,   0
        DCA STR_PTR
PUTS_LOOP,
        TAD I STR_PTR
        SZA
        JMP PUTS_CHAR
        JMP I PUTS
PUTS_CHAR,
        JMS PUTCH
        ISZ STR_PTR
        JMP PUTS_LOOP

PUTCH,  0
        DCA CHBUF
PUTCH_WAIT,
        IOT 6041
        JMP PUTCH_WAIT
        TAD CHBUF
        IOT 6046
        CLA CLL
        JMP I PUTCH

/ ------------------------------------------------------------
/ Main program
/ ------------------------------------------------------------
        *0200
START,
        CLA CLL
        TAD HEADER_PTR
        JMS PUTS
        CLA CLL
        TAD ALIGN_PTR
        JMS PUTS

        CLA CLL
        TAD WD_CFG
        IOT 6552

/ Wait for the next watchdog tick to align the sampling window.
ALIGN_WAIT,
        IOT 6551
        JMP ALIGN_WAIT
        IOT 6554            / clear flag & start 2s window

        CLA CLL
        DCA COUNT_LO
        DCA COUNT_MI
        DCA COUNT_HI

/ ------------------------------------------------------------
/ Measurement loop: increment 36-bit counter until next tick.
/ ------------------------------------------------------------
MEASURE_LOOP,
        ISZ COUNT_LO
        JMP CHECK_TICK
        ISZ COUNT_MI
        JMP CHECK_TICK
        ISZ COUNT_HI

CHECK_TICK,
        IOT 6551
        JMP MEASURE_LOOP

MEASURE_DONE,
        IOT 6554            / acknowledge tick so the next sample can run

        CLA CLL
        TAD COUNT_LO
        DCA CPS_LO
        CLA CLL
        TAD COUNT_MI
        DCA CPS_MI
        CLA CLL
        TAD COUNT_HI
        DCA CPS_HI

        CLA CLL
        TAD CPS_HI
        RAR
        DCA CPS_HI
        TAD CPS_MI
        RAR
        DCA CPS_MI
        TAD CPS_LO
        RAR
        DCA CPS_LO

        CLA CLL
        TAD RAW_PTR
        JMS PUTS
        CLA CLL
        TAD COUNT_HI
        JMS PRINT_OCTAL
        CLA CLL
        TAD QUOTE_CHAR
        JMS PUTCH
        CLA CLL
        TAD COUNT_MI
        JMS PRINT_OCTAL
        CLA CLL
        TAD QUOTE_CHAR
        JMS PUTCH
        CLA CLL
        TAD COUNT_LO
        JMS PRINT_OCTAL
        CLA CLL
        TAD CRLF_PTR
        JMS PUTS

        CLA CLL
        TAD SEC_PTR
        JMS PUTS
        CLA CLL
        TAD CPS_HI
        JMS PRINT_OCTAL
        CLA CLL
        TAD QUOTE_CHAR
        JMS PUTCH
        CLA CLL
        TAD CPS_MI
        JMS PRINT_OCTAL
        CLA CLL
        TAD QUOTE_CHAR
        JMS PUTCH
        CLA CLL
        TAD CPS_LO
        JMS PRINT_OCTAL
        CLA CLL
        TAD CRLF_PTR
        JMS PUTS

        HLT

/ ------------------------------------------------------------
/ Strings
/ ------------------------------------------------------------
        *0500
HEADER_STR,
        "w";"d";"-";"p";"e";"r";"f";" ";"d";"e";"m";"o";0015;0012;0000

ALIGN_STR,
        "A";"l";"i";"g";"n";"i";"n";"g";" ";"w";"i";"t";"h";" ";"w";"a";
        "t";"c";"h";"d";"o";"g";" ";"w";"i";"n";"d";"o";"w";".";".";".";0015;0012;0000

RAW_STR,
        "C";"y";"c";"l";"e";"s";" ";"i";"n";" ";"2";"s";":";" ";0000

SEC_STR,
        "C";"y";"c";"l";"e";"s";0057;"s";"e";"c";":";" ";0000

CRLF_STR,
        0015;0012;0000

        $
