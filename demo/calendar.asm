/ calendar.asm
/ Calendar data and routines for PDP-8 program covering years 1957-2099.
/

        *0020                   / Page 0 workspace and constants

YEAR_VALUE,     0000
YEAR_DELTA,     0000
TEMP_OFF,       0000
PTR,            0000
TEMPLATE_IDX,   0000
TEMPLATE_PTR,   0000
MONTH_VALUE,    0000
MONTH_OFF,      0000
MONTH_START,    0000
MONTH_LEN,      0000
LEAP_FLAG,      0000
WEEKDAY,        0000
BLANK_COUNT,    0000
DAY_NUM,        0000
DAYS_LEFT,      0000
YEAR_TMP,       0000
REM_VALUE,      0000
TENS,           0000
DAY_TEMP,       0000
PRT2D_WORK,     0000
LEAD_CHAR,      0000
CHBUF,          0000
TEST_PTR,       0000
SMOKE_YEAR,     03645

YEAR_IN_PTR,    YEAR_IN
MONTH_IN_PTR,   MONTH_IN
YEAR_MAP_PTR,   YEAR_MAP
CAL_PTRS_PTR,   CAL_PTRS
MONTHLEN_COMM_PTR, MONTHLEN_COMMON
MONTHLEN_LEAP_PTR, MONTHLEN_LEAP
MONTH_PTRS_PTR, MONTH_NAME_PTRS
DAY_HDR_PTR,    DAY_HDR

NEG_BASE,       04133           / -1957
NEG_28,         07744           / -28
NEG_ONE,        07777           / -1
NEG_SEVEN,      07771           / -7
NEG_10,         07766           / -10
NEG_1900,       04224           / -1900
NEG_2000,       04060           / -2000
MASK003,        0003
ONE,            0001
TWO,            0002
NINE,           0011
ZERO,           0060
SPACE,          0040
CR,             0015
LF,             0012

PRCAL,  0
        JMP I PRCAL_ADR
PRCAL_ADR,     PRCAL_IMPL
PRCAL_DONE_PTR,PRCAL_DONE
DAY_LOOP_PTR,  DAY_LOOP
DAY_WORK_PTR,  DAY_WORK

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

NEWLN,  0
        JMP I NEWLN_ADR
NEWLN_ADR,     NEWLN_IMPL

        *0200                   / Page 1: inputs and lookup tables

YEAR_IN,        0000            / Requested year (1957-2099 inclusive)
MONTH_IN,       0000            / Requested month (1-12)

/ Pointer table into the 14 calendar templates.
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

        *0400                   / Page 2: pattern data (common set)

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

        *0600                   / Page 3: pattern data (remaining set)

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

        *0700                   / Page 4: month name and header strings

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

DAY_HDR,"S";"u";" ";"M";"o";" ";"T";"u";" ";"W";"e";" ";"T";"h";" ";"F";"r";" ";"S";"a";0015;0012;0000

        *1000                   / Application code entry point

START,  CLA CLL
        DCA YEAR_VALUE
        DCA YEAR_DELTA
        DCA TEMP_OFF
        DCA PTR
        DCA TEMPLATE_IDX
        DCA TEMPLATE_PTR
        DCA MONTH_VALUE
        DCA MONTH_OFF
        DCA MONTH_START
        DCA MONTH_LEN
        DCA LEAP_FLAG
        DCA WEEKDAY
        DCA BLANK_COUNT
        DCA DAY_NUM
        DCA DAYS_LEFT
        DCA YEAR_TMP
        DCA REM_VALUE
        DCA TENS
        DCA DAY_TEMP
        DCA LEAD_CHAR
        DCA CHBUF

        CLA
        TAD YEAR_IN_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA YEAR_VALUE

        CLA
        TAD YEAR_VALUE
        TAD NEG_BASE
        DCA YEAR_DELTA

MOD28,  CLA
        TAD YEAR_DELTA
        TAD NEG_28
        SPA                     / Keep subtracting while result stays non-negative
        JMP MOD28_DONE
        DCA YEAR_DELTA
        JMP MOD28

MOD28_DONE,
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
        TAD I PTR
        DCA MONTH_START

        CLA
        TAD YEAR_VALUE
        AND MASK003
        SZA
        JMP NOT_LEAP

LEAP_PATH,
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
        JMP PRINT_READY

NOT_LEAP,
        CLA
        TAD MONTH_OFF
        TAD MONTHLEN_COMM_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA MONTH_LEN

PRINT_READY,
        JMS PRCAL

DONE,   HLT
        JMP START

        *1100

PRCAL_IMPL,  0                  / Print calendar for selected month
        JMS NEWLN

        CLA
        TAD MONTH_OFF
        TAD MONTH_PTRS_PTR
        DCA PTR
        CLA
        TAD I PTR
        DCA PTR
        JMS PRINT              / Month name

        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

        CLA
        TAD YEAR_VALUE
        DCA YEAR_TMP
        JMS PRTYEAR

        JMS NEWLN

        CLA
        TAD DAY_HDR_PTR
        DCA PTR
        JMS PRINT

        CLA
        TAD SPACE
        DCA LEAD_CHAR

        CLA
        TAD MONTH_START
        DCA WEEKDAY
        CLA
        TAD MONTH_START
        DCA BLANK_COUNT

BLANK_LOOP,
        CLA
        TAD BLANK_COUNT
        SZA
        JMP BLANK_DO
        JMP BLANK_DONE

BLANK_DO,
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

        CLA
        TAD BLANK_COUNT
        TAD NEG_ONE
        DCA BLANK_COUNT
        JMP BLANK_LOOP

BLANK_DONE,
        CLA
        TAD MONTH_LEN
        DCA DAYS_LEFT
        CLA
        DCA DAY_NUM

DAY_LOOP,
        CLA
        TAD DAYS_LEFT
        SZA
        JMP I DAY_WORK_PTR
        JMP I PRCAL_DONE_PTR

DAY_WORK,
        ISZ DAY_NUM

        CLA
        TAD ZERO
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD ZERO
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

        CLA
        TAD DAYS_LEFT
        TAD NEG_ONE
        DCA DAYS_LEFT

        CLA
        TAD WEEKDAY
        TAD ONE
        DCA WEEKDAY

        CLA
        TAD WEEKDAY
        TAD NEG_SEVEN
        SPA
        JMP DAY_NEXT
        DCA WEEKDAY
        JMS NEWLN

DAY_NEXT,
        JMP I DAY_LOOP_PTR

PRCAL_DONE,
        CLA
        TAD WEEKDAY
        SZA
        JMP NEED_FINAL_NL
        JMP NO_EXTRA_NL

NEED_FINAL_NL,
        JMS NEWLN

NO_EXTRA_NL,
        JMS NEWLN
        JMP I PRCAL

        *1200

PRT2D_IMPL, 0                   / Print value in DAY_TEMP as two digits
        CLA
        TAD DAY_TEMP
        DCA PRT2D_WORK
        CLA
        DCA TENS

P2D_LOOP,
        CLA
        TAD PRT2D_WORK
        TAD NEG_10
        SMA
        JMP P2D_DONE
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

        *1300

PRTYEAR_IMPL,0                 / Print year in decimal (1957-2099)
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

        *1400

PRINT_IMPL,  0                  / Print null-terminated string (pointer preloaded in PTR)
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

        *1500

PUTCH_IMPL, 0                   / Output character in CHBUF to line printer (660x)
PUTCH_WAIT,
        IOT 6601
        JMP PUTCH_WAIT
        CLA
        TAD CHBUF
        IOT 6606
        CLA
        JMP I PUTCH

NEWLN_IMPL, 0
        CLA
        TAD CR
        DCA CHBUF
        JMS PUTCH
        CLA
        TAD LF
        DCA CHBUF
        JMS PUTCH
        JMP I NEWLN

        *1600

SMOKE_TEST, 0                   / Smoke test: print "January 1957"
        CLA CLL
        TAD MONTH_PTRS_PTR
        DCA TEST_PTR

        CLA
        TAD I TEST_PTR
        DCA PTR
        JMS PRINT

        CLA
        TAD SPACE
        DCA CHBUF
        JMS PUTCH

        CLA
        TAD SMOKE_YEAR
        DCA YEAR_VALUE

        CLA
        TAD SMOKE_YEAR
        DCA YEAR_TMP

        CLA
        TAD SMOKE_YEAR_STR_PTR
        DCA PTR
        JMS PRINT

        JMS NEWLN
        HLT
        JMP SMOKE_TEST

SMOKE_YEAR_STR_PTR, SMOKE_YEAR_STR
SMOKE_YEAR_STR, "1";"9";"5";"7";0000
