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
MONTH_START,    0000
YEAR_DELTA,     0000
TEMP_OFF,       0000
TEMPLATE_IDX,   0000
TEMPLATE_PTR,   0000
MONTH_LEN,      0000
LEAP_FLAG,      0000
FIRST_DAY,      0000
FIRST_DAY_PTR,  0000
CUR_WEEK_PTR,   0000
PRINT_COL,      0000
WEEK_REMAIN,    0000
CUR_DAY,        0000
DAY_REMAIN,     0000
WEEK_GAP,       0000
WEEK_COUNT,     0000
FORMAT_FLAGS,   0000

YEAR_IN_PTR,    YEAR_IN
MONTH_IN_PTR,   MONTH_IN
MONTH_PTRS_PTR, MONTH_NAME_PTRS
YEAR_MAP_PTR,   YEAR_MAP
CAL_PTRS_PTR,   CAL_PTRS
MONTHLEN_COMM_PTR, MONTHLEN_COMMON
MONTHLEN_LEAP_PTR, MONTHLEN_LEAP
DOW_PTRS_PTR,   DOW_PTRS
DAY_HDR_PTR,    DAY_HDR

NEG_ONE,        07777
NEG_10,         07766
NEG_1900,       04224
NEG_2000,       04060
NEG_BASE,       04133
NEG_28,         07744
NEG_SIX,        07772
NEG_SEVEN,      07771
NEG_EIGHT,      07770
MASK003,        0003
ZERO,           0060
ONE,            0001
TWO,            0002
NINE,           0011
SPACE,          0040
CR,             0015
LF,             0012

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

CALC, 0
        JMP I CALC_ADR
CALC_ADR, CALC_IMPL

FIRSTDAY, 0
        JMP I FIRSTDAY_ADR
FIRSTDAY_ADR, FIRSTDAY_IMPL

PRINT_HEADER, 0
        JMP I PRINT_HEADER_ADR
PRINT_HEADER_ADR, PRINT_HEADER_IMPL

PRINT_WEEK, 0
        JMP I PRINT_WEEK_ADR
PRINT_WEEK_ADR, PRINT_WEEK_IMPL

PRINT_BLANK, 0
        JMP I PRINT_BLANK_ADR
PRINT_BLANK_ADR, PRINT_BLANK_IMPL

PRINT_CALENDAR, 0
        JMP I PRINT_CALENDAR_ADR
PRINT_CALENDAR_ADR, PRINT_CALENDAR_IMPL

        *0200                   / Page 1: program entry

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
        DCA CUR_WEEK_PTR
        DCA PRINT_COL
        DCA WEEK_REMAIN
        DCA CUR_DAY
        DCA DAY_REMAIN
        DCA WEEK_GAP
        DCA WEEK_COUNT
        DCA FORMAT_FLAGS

        / Inline error characters placed in page 1 so direct TAD works
ERR_Q1,  0077
ERR_Q2,  0077
ERR_Q3,  0077
NEG_12, 07764

        CLA                     / Load requested year from S (switch register)
        OSR
        DCA YEAR_VALUE

        CLA                     / Load requested month (1-12)
        TAD MONTH_IN_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA MONTH_VALUE

        / Validate month in range 1..12
        CLA
        TAD MONTH_VALUE
        TAD NEG_ONE
        SMA
        JMP MONTH_OK1
        / inline BAD_MONTH_PRINT: emit three '?' characters then halt
        CLA
        TAD ERR_Q1
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD ERR_Q2
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD ERR_Q3
        DCA CHBUF
        JMS PUTCH
        HLT
MONTH_OK1,
        CLA
        TAD MONTH_VALUE
        TAD NEG_12        / NEG_12 (octal)
        TAD NEG_ONE       / Shift threshold so month == 12 passes
        SPA
        JMP MONTH_OK2
        / inline BAD_MONTH_PRINT: emit three '?' characters then halt
        CLA
        TAD ERR_Q1
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD ERR_Q2
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD ERR_Q3
        DCA CHBUF
        JMS PUTCH
        HLT
MONTH_OK2,

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

        / Compute template index and month start/length
        JMS CALC               / compute MONTH_START and MONTH_LEN
        JMS FIRSTDAY           / derive weekday for month start

        / Print space, weekday name, space, and month length
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

        CLA
        TAD FIRST_DAY_PTR
        DCA PTR
        JMS PRINT

        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

        CLA
        TAD MONTH_LEN
        DCA DAY_TEMP
        JMS PRT2D

        JMS PRINT_HEADER
        JMS PRINT_CALENDAR

        HLT                     / Done
        JMP START               / Simple restart loop

        / Bad-month pointer table and string must live in this page so
        / direct addressing from the start routine works. Place them
        / before the PRT2D implementation (page 5 area).
    

/------------------------------------------------------------
/ PRT2D - Print value in DAY_TEMP as two decimal digits
/------------------------------------------------------------
        *400                   / Page 2: subroutine implementations

CALC_IMPL, 0
        / Compute template index and month start/length
        CLA
        TAD YEAR_VALUE
        TAD NEG_BASE
        DCA YEAR_DELTA

MOD28C,
        CLA
        TAD YEAR_DELTA
        TAD NEG_28
        SPA
        JMP MOD28_DONE_C
        DCA YEAR_DELTA
        JMP MOD28C

MOD28_DONE_C,
        CLA
        TAD YEAR_DELTA
        DCA TEMP_OFF

        CLA
        TAD YEAR_MAP_PTR
        TAD TEMP_OFF
        DCA PTR
        CLA
        TAD I PTR
        DCA TEMPLATE_IDX

        CLA
        TAD TEMPLATE_IDX
        DCA TEMP_OFF

        CLA
        TAD CAL_PTRS_PTR
        TAD TEMP_OFF
        DCA PTR
        CLA
        TAD I PTR
        DCA TEMPLATE_PTR

        CLA
        TAD MONTH_IN_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA MONTH_VALUE

        CLA
        TAD MONTH_VALUE
        TAD NEG_ONE
        DCA MONTH_OFF

        CLA
        TAD MONTH_OFF
        TAD TEMPLATE_PTR
        DCA PTR
        CLA
        TAD PTR
        DCA CUR_WEEK_PTR
        CLA
        TAD I PTR
        DCA MONTH_START

        CLA
        TAD YEAR_VALUE
        AND MASK003
        SZA
        JMP NOT_LEAP2

LEAP2,
        CLA
        TAD ONE
        DCA LEAP_FLAG

        CLA
        TAD MONTH_OFF
        TAD MONTHLEN_LEAP_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA MONTH_LEN
        JMP GOT_LEN

NOT_LEAP2,
        CLA
        TAD MONTH_OFF
        TAD MONTHLEN_COMM_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA MONTH_LEN

GOT_LEN,

        JMP I CALC

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

        *600                   / Page 3: year formatting helpers

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

/------------------------------------------------------------
/ PRINT - Print null-terminated string pointed to by PTR
/------------------------------------------------------------
        *0700
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

/------------------------------------------------------------
/ FIRSTDAY - Determine weekday for first day of selected month
/------------------------------------------------------------
        *01000                  / Page 4: weekday calculation helpers

FIRSTDAY_IMPL,0
        CLA
        TAD MONTH_START
        DCA FIRST_DAY
        CLA
        TAD DOW_PTRS_PTR
        TAD FIRST_DAY
        DCA PTR
        CLA
        TAD I PTR
        DCA FIRST_DAY_PTR
        JMP I FIRSTDAY

        *01100                  / Page 4 continued: calendar printing helpers

PRINT_HEADER_IMPL,0
        CLA
        TAD CR
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD LF
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD DAY_HDR_PTR
        DCA PTR
        JMS PRINT
        JMP I PRINT_HEADER

        *01200

PRINT_BLANK_IMPL,0
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH
        JMP I PRINT_BLANK

        *01300

PRINT_WEEK_IMPL,0
        CLA
        DCA PRINT_COL
        CLA
        TAD NEG_EIGHT
        DCA WEEK_REMAIN

PRINT_WEEK_LOOP,
        ISZ WEEK_REMAIN
        JMP PRINT_WEEK_BODY
        JMP PRINT_WEEK_DONE

PRINT_WEEK_BODY,
        CLA
        TAD PRINT_COL
        SZA
        JMP PRINT_WEEK_SPACE
        JMP PRINT_WEEK_CELL

PRINT_WEEK_SPACE,
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

PRINT_WEEK_CELL,
        CLA
        TAD WEEK_GAP
        SZA
        JMP PRINT_WEEK_FROM_GAP
        JMP PRINT_WEEK_CHECK_DAY

PRINT_WEEK_FROM_GAP,
        JMS PRINT_BLANK
        CLA
        TAD WEEK_GAP
        TAD NEG_ONE
        DCA WEEK_GAP
        JMP PRINT_WEEK_ADVANCE

PRINT_WEEK_CHECK_DAY,
        CLA
        TAD DAY_REMAIN
        SZA
        JMP PRINT_WEEK_HAVE_DAY
        JMS PRINT_BLANK
        JMP PRINT_WEEK_ADVANCE

PRINT_WEEK_HAVE_DAY,
        CLA
        TAD CUR_DAY
        DCA DAY_TEMP
        JMS PRT2D
        ISZ CUR_DAY
        CLA
        TAD DAY_REMAIN
        TAD NEG_ONE
        DCA DAY_REMAIN

PRINT_WEEK_ADVANCE,
        ISZ PRINT_COL
        JMP PRINT_WEEK_LOOP

PRINT_WEEK_DONE,
        CLA
        DCA WEEK_GAP
        CLA
        TAD CR
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD LF
        DCA CHBUF
        JMS PUTCH
        JMP I PRINT_WEEK

        *01400

PRINT_CALENDAR_IMPL,0
        CLA
        TAD SPACE
        DCA LEAD_CHAR

        CLA
        TAD ONE
        DCA CUR_DAY

        CLA
        TAD MONTH_LEN
        DCA DAY_REMAIN

        CLA
        TAD MONTH_START
        DCA WEEK_GAP

        CLA
        TAD NEG_SIX
        DCA WEEK_COUNT

PRINT_CAL_LOOP,
        CLA
        TAD DAY_REMAIN
        SZA
        JMP PRINT_CAL_BODY
        CLA
        TAD WEEK_GAP
        SZA
        JMP PRINT_CAL_BODY
        JMP PRINT_CAL_DONE

PRINT_CAL_BODY,
        JMS PRINT_WEEK
        ISZ WEEK_COUNT
        JMP PRINT_CAL_LOOP
        JMP PRINT_CAL_DONE

PRINT_CAL_DONE,
        JMP I PRINT_CALENDAR

        *02000                   / Page 8: input slots and pointer tables

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

/ Pointer table into the 14 calendar templates (patterns PAT0..PAT13)
CAL_PTRS,
        PAT0
        PAT1
        PAT2
        PAT3
        PAT4
        PAT5
        PAT6
        PAT7
        PAT8
        PAT9
        PAT10
        PAT11
        PAT12
        PAT13

/ Map (year - 1957) mod 28 to template index.
YEAR_MAP,
        0000    / 1957
        0001    / 1958
        0002    / 1959
        0003    / 1960
        0004    / 1961
        0005    / 1962
        0000    / 1963
        0006    / 1964
        0007    / 1965
        0010    / 1966
        0004    / 1967
        0011    / 1968
        0001    / 1969
        0002    / 1970
        0007    / 1971
        0012    / 1972
        0005    / 1973
        0000    / 1974
        0001    / 1975
        0013    / 1976
        0010    / 1977
        0004    / 1978
        0005    / 1979
        0014    / 1980
        0002    / 1981
        0007    / 1982
        0010    / 1983
        0015    / 1984

/ Common and leap-year month lengths (decimal 31/30/28/29 encoded in octal).
MONTHLEN_COMMON,
        0037
        0034
        0037
        0036
        0037
        0036
        0037
        0037
        0036
        0037
        0036
        0037

MONTHLEN_LEAP,
        0037
        0035
        0037
        0036
        0037
        0036
        0037
        0037
        0036
        0037
        0036
        0037

        *02300                   / Pattern data (common set)

PAT0,   0002    / Common year pattern
        0005
        0005
        0001
        0003
        0006
        0001
        0004
        0000
        0002
        0005
        0000

PAT1,   0003    / Common year pattern
        0006
        0006
        0002
        0004
        0000
        0002
        0005
        0001
        0003
        0006
        0001

PAT2,   0004    / Common year pattern
        0000
        0000
        0003
        0005
        0001
        0003
        0006
        0002
        0004
        0000
        0002

PAT3,   0005    / Leap year pattern
        0001
        0002
        0005
        0000
        0003
        0005
        0001
        0004
        0006
        0002
        0004

PAT4,   0000    / Common year pattern
        0003
        0003
        0006
        0001
        0004
        0006
        0002
        0005
        0000
        0003
        0005

PAT5,   0001    / Common year pattern
        0004
        0004
        0000
        0002
        0005
        0000
        0003
        0006
        0001
        0004
        0006

PAT6,   0003    / Leap year pattern
        0006
        0000
        0003
        0005
        0001
        0003
        0006
        0002
        0004
        0000
        0002

PAT7,   0005    / Common year pattern
        0001
        0001
        0004
        0006
        0002
        0004
        0000
        0003
        0005
        0001
        0003

PAT8,   0006    / Common year pattern
        0002
        0002
        0005
        0000
        0003
        0005
        0001
        0004
        0006
        0002
        0004

PAT9,   0001    / Leap year pattern
        0004
        0005
        0001
        0003
        0006
        0001
        0004
        0000
        0002
        0005
        0000

PAT10,  0006    / Leap year pattern
        0002
        0003
        0006
        0001
        0004
        0006
        0002
        0005
        0000
        0003
        0005

PAT11,  0004    / Leap year pattern
        0000
        0001
        0004
        0006
        0002
        0004
        0000
        0003
        0005
        0001
        0003

PAT12,  0002    / Leap year pattern
        0005
        0006
        0002
        0004
        0000
        0002
        0005
        0001
        0003
        0006
        0001

PAT13,  0000    / Leap year pattern
        0003
        0004
        0000
        0002
        0005
        0000
        0003
        0006
        0001
        0004
        0006

DAY_HDR,"S";"u";" ";"M";"o";" ";"T";"u";" ";"W";"e";" ";"T";"h";" ";"F";"r";" ";"S";"a";0015;0012;0000

DOW_PTRS,
        SUN_STR
        MON_STR
        TUE_STR
        WED_STR
        THU_STR
        FRI_STR
        SAT_STR

SUN_STR,"S";"u";"n";"d";"a";"y";0000
MON_STR,"M";"o";"n";"d";"a";"y";0000
TUE_STR,"T";"u";"e";"s";"d";"a";"y";0000
WED_STR,"W";"e";"d";"n";"e";"s";"d";"a";"y";0000
THU_STR,"T";"h";"u";"r";"s";"d";"a";"y";0000
FRI_STR,"F";"r";"i";"d";"a";"y";0000
SAT_STR,"S";"a";"t";"u";"r";"d";"a";"y";0000
