/ cal1970.asm
/ Print the calendar for January 1970 on the KL8E console, then halt.
/

        *0200                   / Program origin

START,  CLA CLL
        TAD HEADER_PTR
        JMS PRINT
        TAD WEEK_PTR
        JMS PRINT
        TAD LINE1_PTR
        JMS PRINT
        TAD LINE2_PTR
        JMS PRINT
        TAD LINE3_PTR
        JMS PRINT
        TAD LINE4_PTR
        JMS PRINT
        TAD LINE5_PTR
        JMS PRINT
        HLT

/------------------------------------------------------------
/ PRINT - Null-terminated string printer.
/ Entry: AC = pointer to string.
/------------------------------------------------------------
PRINT,  0
        DCA PTR
PR_NEXT,
        TAD I PTR
        SZA
        JMP PR_CONT
        JMP I PRINT
PR_CONT,
        JMS PUTCH
        ISZ PTR
        JMP PR_NEXT

/------------------------------------------------------------
/ PUTCH - Output character in AC via KL8E.
/------------------------------------------------------------
PUTCH,  0
        DCA CHBUF
PUT_WAIT,
        IOT 6041                / Skip when teleprinter ready
        JMP PUT_WAIT
        TAD CHBUF
        IOT 6046                / Transmit character
        CLA
        JMP I PUTCH

/------------------------------------------------------------
/ Data tables
/------------------------------------------------------------
PTR,            0000
CHBUF,          0000

HEADER_PTR,     HEADER
WEEK_PTR,       WEEK_HDR
LINE1_PTR,      LINE1
LINE2_PTR,      LINE2
LINE3_PTR,      LINE3
LINE4_PTR,      LINE4
LINE5_PTR,      LINE5

HEADER, "J";"a";"n";"u";"a";"r";"y";" "; "1";"9";"7";"0";0015;0012;0000
WEEK_HDR,"S";"u";" ";"M";"o";" ";"T";"u";" ";"W";"e";" ";"T";"h";" ";"F";"r";" ";"S";"a";0015;0012;0000

LINE1,  0040;0040;0040;0040;0040;0040;0040;0040;0040;0040;0040;0040;
        0040;"1";0040;0040;"2";0040;0040;"3";0040;0015;0012;0000
LINE2,  0040;"4";0040;0040;"5";0040;0040;"6";0040;0040;"7";0040;
        0040;"8";0040;0040;"9";0040;"1";"0";0040;0015;0012;0000
LINE3,  "1";"1";0040;"1";"2";0040;"1";"3";0040;"1";"4";0040;"1";"5";0040;"1";"6";0040;"1";"7";0040;0015;0012;0000
LINE4,  "1";"8";0040;"1";"9";0040;"2";"0";0040;"2";"1";0040;"2";"2";0040;"2";"3";0040;"2";"4";0040;0015;0012;0000
LINE5,  "2";"5";0040;"2";"6";0040;"2";"7";0040;"2";"8";0040;"2";"9";0040;"3";"0";0040;"3";"1";0040;0015;0012;0000

        $                       / End of program
