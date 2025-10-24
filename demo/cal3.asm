/ cal3.asm
/ Minimal calendar printer: emit selected year and month name then halt.

        *0020                   / Page 0 workspace and constants

YEAR_VALUE,     0000
MONTH_VALUE,    0000
MONTH_OFF,      0000
YEAR_TMP,       0000
REM_VALUE,      0000
TENS,           0000
PRT2D_WORK,     0000
LEAD_CHAR,      0000
DAY_TEMP,       0000
CHBUF,          0000
PTR,            0000

YEAR_IN_PTR,    YEAR_IN
MONTH_IN_PTR,   MONTH_IN
MONTH_PTRS_PTR, MONTH_NAME_PTRS

NEG_ONE,        07777
NEG_10,         07766
NEG_1900,       04224
NEG_2000,       04060
ZERO,           0060
ONE,            0001
TWO,            0002
NINE,           0011
SPACE,          0040

PRT2D,  0
        JMP I PRT2D_ADR
PRT2D_ADR,     PRT2D_IMPL

PRTYEAR,0
        JMP I PRTYEAR_ADR
PRTYEAR_ADR,   PRTYEAR_IMPL

PRINT,  0
        JMP I PRINT_ADR
PRINT_ADR,     PRINT_IMPL

PUTCH,  0
        JMP I PUTCH_ADR
PUTCH_ADR,     PUTCH_IMPL

        *0100                   / Page 1: program entry

START,  CLA CLL
        DCA YEAR_VALUE
        DCA MONTH_VALUE
        DCA MONTH_OFF
        DCA YEAR_TMP
        DCA REM_VALUE
        DCA TENS
        DCA PRT2D_WORK
        DCA LEAD_CHAR
        DCA DAY_TEMP
        DCA CHBUF
        DCA PTR

        CLA                     / Load requested year from S (switch register)
        OSR
        DCA YEAR_VALUE

        CLA                     / Load requested month (1-12)
        TAD MONTH_IN_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA MONTH_VALUE

        CLA                     / Copy year for printing
        TAD YEAR_VALUE
        DCA YEAR_TMP
        JMS PRTYEAR             / Print decimal year

        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH               / Space separator

        CLA                     / Convert month to zero-based offset
        TAD MONTH_VALUE
        TAD NEG_ONE
        DCA MONTH_OFF

        CLA                     / Fetch pointer to month name string
        TAD MONTH_PTRS_PTR
        TAD MONTH_OFF
        DCA PTR
        CLA
        TAD I PTR
        DCA PTR
        JMS PRINT               / Print month name

        HLT                     / Done
        JMP START               / Simple restart loop

/------------------------------------------------------------
/ PRT2D - Print value in DAY_TEMP as two decimal digits
/------------------------------------------------------------
        *0200

PRT2D_IMPL, 0
        CLA
        TAD DAY_TEMP
        DCA PRT2D_WORK
        CLA
        DCA TENS

P2D_LOOP,
        CLA
        TAD PRT2D_WORK
        TAD NEG_10
        SMA                     / skip when AC is negative  
        JMP CONTINUE           / Continue when AC >= 0
        JMP P2D_DONE           / Exit when AC < 0

CONTINUE,
        DCA PRT2D_WORK
        ISZ TENS
        JMP P2D_LOOP

P2D_DONE,
        CLA
        TAD TENS
        SZA
        JMP P2D_PRINT_TENS

        CLA
        TAD LEAD_CHAR
        DCA CHBUF
        JMS PUTCH
        JMP P2D_ONES

P2D_PRINT_TENS,
        CLA
        TAD TENS
        TAD ZERO
        DCA CHBUF
        JMS PUTCH

P2D_ONES,
        CLA
        TAD PRT2D_WORK
        TAD ZERO
        DCA CHBUF
        JMS PUTCH
        JMP I PRT2D

        *0300

/------------------------------------------------------------
/ PRTYEAR - Print YEAR_TMP in decimal (1957-2099)
/------------------------------------------------------------
PRTYEAR_IMPL,0
        CLA
        TAD YEAR_TMP
        TAD NEG_2000
        SMA
        JMP YEAR_GE_2000
        JMP YEAR_LT_2000

YEAR_LT_2000,
        CLA
        TAD ZERO
        TAD ONE
        DCA CHBUF
        JMS PUTCH              / '1'

        CLA
        TAD ZERO
        TAD NINE
        DCA CHBUF
        JMS PUTCH              / '9'

        CLA
        TAD YEAR_TMP
        TAD NEG_1900
        DCA REM_VALUE

        CLA
        TAD ZERO
        DCA LEAD_CHAR          / Leading zero padding ('0')

        CLA
        TAD REM_VALUE
        DCA DAY_TEMP
        JMS PRT2D
        JMP YEAR_DONE

YEAR_GE_2000,
        CLA
        TAD ZERO
        TAD TWO
        DCA CHBUF
        JMS PUTCH              / '2'

        CLA
        TAD ZERO
        DCA CHBUF
        JMS PUTCH              / '0'

        CLA
        TAD YEAR_TMP
        TAD NEG_2000
        DCA REM_VALUE

        CLA
        TAD ZERO
        DCA LEAD_CHAR

        CLA
        TAD REM_VALUE
        DCA DAY_TEMP
        JMS PRT2D

YEAR_DONE,
        JMP I PRTYEAR

        *0400

/------------------------------------------------------------
/ PRINT - Print null-terminated string pointed to by PTR
/------------------------------------------------------------
PRINT_IMPL, 0
PRT_LOOP,
        CLA
        TAD I PTR
        SZA
        JMP PRT_CHAR
        JMP I PRINT

PRT_CHAR,
        DCA CHBUF
        JMS PUTCH
        ISZ PTR
        JMP PRT_LOOP

        *0500

/------------------------------------------------------------
/ PUTCH - Output character in CHBUF to the line printer (660x)
/------------------------------------------------------------
PUTCH_IMPL,0
PUTCH_WAIT,
        IOT 6601
        JMP PUTCH_WAIT
        CLA
        TAD CHBUF
        IOT 6606
        CLA
        JMP I PUTCH

        *0600                   / Page 6: input slots and pointer tables

YEAR_IN,        0000            / Requested year (1957-2099 inclusive)
MONTH_IN,       0000            / Requested month (1-12)

MONTH_NAME_PTRS,
        JAN_STR
        FEB_STR
        MAR_STR
        APR_STR
        MAY_STR
        JUN_STR
        JUL_STR
        AUG_STR
        SEP_STR
        OCT_STR
        NOV_STR
        DEC_STR

        *0700                   / Page 7: month name strings

JAN_STR,"J";"a";"n";"u";"a";"r";"y";0000
FEB_STR,"F";"e";"b";"r";"u";"a";"r";"y";0000
MAR_STR,"M";"a";"r";"c";"h";0000
APR_STR,"A";"p";"r";"i";"l";0000
MAY_STR,"M";"a";"y";0000
JUN_STR,"J";"u";"n";"e";0000
JUL_STR,"J";"u";"l";"y";0000
AUG_STR,"A";"u";"g";"u";"s";"t";0000
SEP_STR,"S";"e";"p";"t";"e";"m";"b";"e";"r";0000
OCT_STR,"O";"c";"t";"o";"b";"e";"r";0000
NOV_STR,"N";"o";"v";"e";"m";"b";"e";"r";0000
DEC_STR,"D";"e";"c";"e";"m";"b";"e";"r";0000
