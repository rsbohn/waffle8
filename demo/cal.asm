/ cal.asm
/ Prompt for a year and month on the KL8E console and print the calendar.
/

        *0020                   / Zero-page pointer table for subroutines
PRINT_PROMPT_PTR,       PRINT_PROMPT
MAIN_LOOP_PTR,          MAIN_LOOP
READ_NUMBER_PTR,        READ_NUMBER
IS_LEAP_PTR,            IS_LEAP
MOD7_PTR,               MOD7
CONSUME_EOL_PTR,        CONSUME_EOL
PRINT_MONTH_HEADER_PTR, PRINT_MONTH_HEADER
PRINT_WEEK_HEADER_PTR,  PRINT_WEEK_HEADER
PRINT_CAL_BODY_PTR,     PRINT_CALENDAR_BODY
SKIP_LINE_PTR,          SKIP_LINE
PRINT_PTR,              PRINT
PUTCH_PTR,              PUTCH
NEWLINE_PTR,            NEWLINE
PRINT_DAY_FIELD_PTR,    PRINT_DAY_FIELD
MUL10_ADD_DIGIT_PTR,    MUL10_ADD_DIGIT
NEXT_NONSPACE_PTR,      NEXT_NONSPACE
NEXT_CHAR_PTR,          NEXT_CHAR
UNGET_CHAR_PTR,         UNGET_CHAR
READ_CHAR_PTR,          READ_CHAR
GETCH_PTR,              GETCH
MOD400_PTR,             MOD400
MOD100_PTR,             MOD100
MOD4_PTR,               MOD4
PRINT_DECIMAL_PTR,      PRINT_DECIMAL
YEAR_INPUT_ERROR_PTR,   YEAR_INPUT_ERROR
YEAR_RANGE_ERROR_PTR,   YEAR_RANGE_ERROR
MONTH_INPUT_ERROR_PTR,  MONTH_INPUT_ERROR
MONTH_RANGE_ERROR_PTR,  MONTH_RANGE_ERROR
MONTH_NONZERO_PTR,      MONTH_NONZERO
MONTH_VALID_PTR,        MONTH_VALID
YEAR_BODY_PTR,          YEAR_BODY
YEARS_DONE_PTR,         YEARS_DONE
YEAR_LOOP_PTR,          YEAR_LOOP
MONTH_BODY_PTR,         MONTH_BODY
MONTH_DONE_PTR,         MONTH_DONE
AFTER_FEB_CHECK_PTR,    AFTER_FEB_CHECK
MAYBE_LEAP_FEB_PTR,     MAYBE_LEAP_FEB
MONTH_LOOP_PTR,         MONTH_LOOP
AFTER_LENGTH_ADJUST_PTR,AFTER_LENGTH_ADJUST
LEAP_LENGTH_PTR,        LEAP_LENGTH
BLANK_BODY_PTR,         BLANK_BODY
BLANK_DONE_PTR,         BLANK_DONE
BLANK_LOOP_PTR,         BLANK_LOOP
DAY_BODY_PTR,           DAY_BODY
DAY_LOOP_PTR,           DAY_LOOP
WRAP_WEEK_PTR,          WRAP_WEEK
CAL_DONE_PTR,           CAL_DONE
FINAL_GAP_PTR,          FINAL_GAP
SKIP_FINAL_GAP_PTR,     SKIP_FINAL_GAP

NUMBER,         0000
CHAR_TMP,       0000
DIGIT,          0000
YEAR,           0000
MONTH,          0000
YEAR_WORK,      0000
YEARS_LEFT,     0000
CUR_YEAR,       0000
DAY_ACCUM,      0000
TEMP_FLAG,      0000
TEMP_DAYS,      0000
MONTH_COUNT,    0000
MONTH_PTR,      0000
MONTH_INDEX,    0000
MONTH_LENGTH,   0000
LEAP_FLAG,      0000
REM_WORK,       0000
DAY_OF_WEEK,    0000
DAYS_LEFT,      0000
DAY_NUMBER,     0000
NAME_PTR,       0000
NAME_INDEX,     0000
TEMP_COUNTER,   0000
DOW_CURSOR,     0000
INCHAR,         0000
OUTBUF,         0000
STR_PTR,        0000
PEEK_CHAR,      0000
NEXT_TMP,       0000
NEWVAL,         0000
OLDVAL,         0000
MUL_COUNT,      0000
REM_DAY,        0000
TENS,           0000
DEC_VALUE,      0000
DEC_STARTED,    0000

WEEK_HDR_PTR,   WEEK_HDR
ERR_INPUT_PTR,  ERR_INPUT
ERR_YEAR_PTR,   ERR_YEAR
ERR_MONTH_PTR,  ERR_MONTH
MONTH_LEN_PTR_BASE, MONTH_LENGTHS
MONTH_NAME_PTR_BASE, MONTH_NAME_PTRS

BASE_YEAR,      03645
BASE_DAY,       0002
ONE,            0001
TEN,            0012
DAYS_365,       00555

NEG_ONE,        07777
NEG_TWO,        07776
NEG_SEVEN,      07771
NEG_TEN,        07766
NEG_13,         07763
NEG_1957,       04133
NEG_ASCII_ZERO, 07720
NEG_ASCII_COLON,07706
NEG_SPACE,      07740
NEG_TAB,        07767

SPACE,          00040
CHAR_PERCENT,   00045
CHAR_CR,        00015
CHAR_LF,        00012
ASCII_ZERO,     00060

        *0200                   / Program origin

START,  CLA CLL                 / Clear AC and link

MAIN_LOOP,
        JMS I PRINT_PROMPT_PTR  / Emit "% "

        JMS I READ_NUMBER_PTR   / Read year
        SZA                     / Success leaves AC = 0 -> skip error jump
        JMP I YEAR_INPUT_ERROR_PTR
        CLA
        TAD NUMBER
        DCA YEAR

        CLA
        TAD YEAR
        TAD NEG_1957
        SMA                     / YEAR - 1957 >= 0? Negative means too small
        JMP I YEAR_RANGE_ERROR_PTR

        JMS I READ_NUMBER_PTR   / Read month
        SZA
        JMP I MONTH_INPUT_ERROR_PTR
        CLA
        TAD NUMBER
        DCA MONTH

        CLA
        TAD MONTH
        SZA                     / Month zero?
        JMP I MONTH_NONZERO_PTR
        JMP I MONTH_RANGE_ERROR_PTR
MONTH_NONZERO,
        CLA
        TAD MONTH
        TAD NEG_13
        SPA                     / Month >= 13?
        JMP I MONTH_VALID_PTR
        JMP I MONTH_RANGE_ERROR_PTR

MONTH_VALID,
        CLA
        TAD YEAR
        DCA YEAR_WORK
        JMS I IS_LEAP_PTR       / Determine leap status of target year
        DCA LEAP_FLAG

        CLA
        DCA DAY_ACCUM
        CLA
        TAD YEAR
        TAD NEG_1957
        DCA YEARS_LEFT
        CLA
        TAD BASE_YEAR
        DCA CUR_YEAR

        CLA
        TAD MONTH_LEN_PTR_BASE
        DCA MONTH_PTR
        CLA
        TAD ONE
        DCA MONTH_INDEX

/ Accumulate days for full years between 1957 and (YEAR - 1)
YEAR_LOOP,
        CLA
        TAD YEARS_LEFT
        SZA
        JMP I YEAR_BODY_PTR
        JMP I YEARS_DONE_PTR
YEAR_BODY,
        CLA
        TAD CUR_YEAR
        DCA YEAR_WORK
        JMS I IS_LEAP_PTR
        DCA TEMP_FLAG
        CLA
        TAD DAYS_365
        TAD TEMP_FLAG
        DCA TEMP_DAYS
        CLA
        TAD DAY_ACCUM
        TAD TEMP_DAYS
        DCA DAY_ACCUM
        ISZ CUR_YEAR
        CLA
        TAD YEARS_LEFT
        TAD NEG_ONE
        DCA YEARS_LEFT
        JMP I YEAR_LOOP_PTR

YEARS_DONE,
        CLA
        TAD MONTH
        TAD NEG_ONE
        DCA MONTH_COUNT         / Number of prior months to add

/ Add days for months preceding the target month
MONTH_LOOP,
        CLA
        TAD MONTH_COUNT
        SZA
        JMP I MONTH_BODY_PTR
        JMP I MONTH_DONE_PTR
MONTH_BODY,
        CLA
        TAD I MONTH_PTR
        DCA TEMP_DAYS
        CLA
        TAD MONTH_INDEX
        TAD NEG_TWO
        SZA
        JMP I AFTER_FEB_CHECK_PTR
        CLA
        TAD LEAP_FLAG
        SZA
        JMP I MAYBE_LEAP_FEB_PTR
        JMP I AFTER_FEB_CHECK_PTR
MAYBE_LEAP_FEB,
        CLA
        TAD TEMP_DAYS
        TAD ONE
        DCA TEMP_DAYS
AFTER_FEB_CHECK,
        CLA
        TAD DAY_ACCUM
        TAD TEMP_DAYS
        DCA DAY_ACCUM
        ISZ MONTH_PTR
        ISZ MONTH_INDEX
        CLA
        TAD MONTH_COUNT
        TAD NEG_ONE
        DCA MONTH_COUNT
        JMP I MONTH_LOOP_PTR

MONTH_DONE,
        CLA
        TAD I MONTH_PTR
        DCA MONTH_LENGTH
        CLA
        TAD MONTH_INDEX
        TAD NEG_TWO
        SZA
        JMP I AFTER_LENGTH_ADJUST_PTR
        CLA
        TAD LEAP_FLAG
        SZA
        JMP I LEAP_LENGTH_PTR
        JMP I AFTER_LENGTH_ADJUST_PTR
LEAP_LENGTH,
        CLA
        TAD MONTH_LENGTH
        TAD ONE
        DCA MONTH_LENGTH
AFTER_LENGTH_ADJUST,
        CLA
        TAD DAY_ACCUM
        DCA REM_WORK
        JMS I MOD7_PTR
        DCA REM_WORK
        CLA
        TAD BASE_DAY
        TAD REM_WORK
        DCA REM_WORK
        JMS I MOD7_PTR
        DCA DAY_OF_WEEK

        CLA
        TAD MONTH_NAME_PTR_BASE
        DCA NAME_PTR
        CLA
        TAD MONTH
        TAD NEG_ONE
        DCA NAME_INDEX

        CLA
        JMS I CONSUME_EOL_PTR   / Remove trailing newline before output

        JMS I PRINT_MONTH_HEADER_PTR
        JMS I PRINT_WEEK_HEADER_PTR
        JMS I PRINT_CAL_BODY_PTR
        JMP I MAIN_LOOP_PTR

YEAR_INPUT_ERROR,
        JMS I SKIP_LINE_PTR
        CLA
        TAD ERR_INPUT_PTR
        JMS I PRINT_PTR
        JMP I MAIN_LOOP_PTR

YEAR_RANGE_ERROR,
        JMS I SKIP_LINE_PTR
        CLA
        TAD ERR_YEAR_PTR
        JMS I PRINT_PTR
        JMP I MAIN_LOOP_PTR

MONTH_INPUT_ERROR,
        JMS I SKIP_LINE_PTR
        CLA
        TAD ERR_INPUT_PTR
        JMS I PRINT_PTR
        JMP I MAIN_LOOP_PTR

MONTH_RANGE_ERROR,
        JMS I SKIP_LINE_PTR
        CLA
        TAD ERR_MONTH_PTR
        JMS I PRINT_PTR
        JMP I MAIN_LOOP_PTR

/------------------------------------------------------------
/ PRINT_MONTH_HEADER - Print month name and year line.
/------------------------------------------------------------
PRINT_MONTH_HEADER, 0
PMH_RECHECK,
        CLA
        TAD NAME_INDEX
        SZA
        JMP NAME_STEP
        JMP NAME_READY
NAME_STEP,
        ISZ NAME_PTR
        CLA
        TAD NAME_INDEX
        TAD NEG_ONE
        DCA NAME_INDEX
        JMP PMH_RECHECK
NAME_READY,
        CLA
        TAD I NAME_PTR
        JMS I PRINT_PTR
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        TAD YEAR
        JMS I PRINT_DECIMAL_PTR
        JMS I NEWLINE_PTR
        CLA
        JMP I PRINT_MONTH_HEADER

/------------------------------------------------------------
/ PRINT_WEEK_HEADER - Emit weekday header line.
/------------------------------------------------------------
PRINT_WEEK_HEADER, 0
        CLA
        TAD WEEK_HDR_PTR
        JMS I PRINT_PTR
        JMS I NEWLINE_PTR
        CLA
        JMP I PRINT_WEEK_HEADER

/------------------------------------------------------------
/ PRINT_CALENDAR_BODY - Print the calendar grid.
/------------------------------------------------------------
PRINT_CALENDAR_BODY, 0
        CLA
        TAD DAY_OF_WEEK
        DCA DOW_CURSOR

        CLA
        TAD ONE
        DCA DAY_NUMBER

        CLA
        DCA TEMP_COUNTER
        CLA
        TAD DOW_CURSOR
        DCA TEMP_COUNTER

BLANK_LOOP,
        CLA
        TAD TEMP_COUNTER
        SZA
        JMP I BLANK_BODY_PTR
        JMP I BLANK_DONE_PTR
BLANK_BODY,
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        TAD TEMP_COUNTER
        TAD NEG_ONE
        DCA TEMP_COUNTER
        JMP I BLANK_LOOP_PTR

BLANK_DONE,
        CLA
        TAD MONTH_LENGTH
        DCA DAYS_LEFT

DAY_LOOP,
        CLA
        TAD DAYS_LEFT
        SZA
        JMP I DAY_BODY_PTR
        JMP I CAL_DONE_PTR
DAY_BODY,
        JMS I PRINT_DAY_FIELD_PTR
        CLA
        TAD DAY_NUMBER
        TAD ONE
        DCA DAY_NUMBER
        CLA
        TAD DAYS_LEFT
        TAD NEG_ONE
        DCA DAYS_LEFT
        CLA
        TAD DOW_CURSOR
        TAD ONE
        DCA DOW_CURSOR
        CLA
        TAD DOW_CURSOR
        TAD NEG_SEVEN
        SMA
        JMP I WRAP_WEEK_PTR
        JMP I DAY_LOOP_PTR
WRAP_WEEK,
        CLA
        DCA DOW_CURSOR
        JMS I NEWLINE_PTR
        JMP I DAY_LOOP_PTR

CAL_DONE,
        CLA
        TAD DOW_CURSOR
        SZA
        JMP I FINAL_GAP_PTR
        JMP I SKIP_FINAL_GAP_PTR
FINAL_GAP,
        JMS I NEWLINE_PTR
SKIP_FINAL_GAP,
        JMS I NEWLINE_PTR
        CLA
        JMP I PRINT_CAL_BODY_PTR

/------------------------------------------------------------
/ PRINT_PROMPT - Emit "% ".
/------------------------------------------------------------
PRINT_PROMPT, 0
        CLA
        TAD CHAR_PERCENT
        JMS I PUTCH_PTR
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        JMP I PRINT_PROMPT

/------------------------------------------------------------
/ READ_NUMBER - Parse unsigned decimal integer into NUMBER.
/ Returns AC=0 on success, AC=1 on failure.
/------------------------------------------------------------
READ_NUMBER, 0
        CLA
        DCA NUMBER

        JMS I NEXT_NONSPACE_PTR
        DCA CHAR_TMP

        CLA
        TAD CHAR_TMP
        TAD NEG_ASCII_ZERO
        SMA
        JMP NUMBER_ERROR
        DCA DIGIT
        CLA
        TAD CHAR_TMP
        TAD NEG_ASCII_COLON
        SPA
        JMP NUMBER_ERROR

        CLA
        TAD DIGIT
        DCA NUMBER

READ_DIGIT_LOOP,
        JMS I NEXT_CHAR_PTR
        DCA CHAR_TMP

        CLA
        TAD CHAR_TMP
        TAD NEG_ASCII_ZERO
        SMA
        JMP DIGIT_END
        DCA DIGIT
        CLA
        TAD CHAR_TMP
        TAD NEG_ASCII_COLON
        SPA
        JMP DIGIT_END

        JMS I MUL10_ADD_DIGIT_PTR
        JMP READ_DIGIT_LOOP

DIGIT_END,
        CLA
        TAD CHAR_TMP
        TAD NEG_SPACE
        SZA
        JMP CHECK_TAB
        JMP DELIM_OK
CHECK_TAB,
        CLA
        TAD CHAR_TMP
        TAD NEG_TAB
        SZA
        JMP CHECK_CR
        JMP DELIM_OK
CHECK_CR,
        CLA
        TAD CHAR_TMP
        TAD NEG_13
        SZA
        JMP CHECK_LF
        JMP DELIM_OK
CHECK_LF,
        CLA
        TAD CHAR_TMP
        TAD NEG_TEN
        SZA
        JMP NUMBER_ERROR_REWIND
        JMP DELIM_OK

DELIM_OK,
        CLA
        TAD CHAR_TMP
        JMS I UNGET_CHAR_PTR
        CLA
        JMP I READ_NUMBER

NUMBER_ERROR_REWIND,
        CLA
        TAD CHAR_TMP
        JMS I UNGET_CHAR_PTR
        CLA
        TAD ONE
        JMP I READ_NUMBER

NUMBER_ERROR,
        CLA
        TAD CHAR_TMP
        JMS I UNGET_CHAR_PTR
        CLA
        TAD ONE
        JMP I READ_NUMBER

/------------------------------------------------------------
/ NEXT_NONSPACE - Fetch next non-whitespace character into AC.
/------------------------------------------------------------
        *0400
NEXT_NONSPACE, 0
NN_LOOP,
        JMS I NEXT_CHAR_PTR
        DCA CHAR_TMP
        CLA
        TAD CHAR_TMP
        TAD NEG_SPACE
        SZA
        JMP CHECK_WS_TAB
        JMP NN_LOOP
CHECK_WS_TAB,
        CLA
        TAD CHAR_TMP
        TAD NEG_TAB
        SZA
        JMP CHECK_WS_CR
        JMP NN_LOOP
CHECK_WS_CR,
        CLA
        TAD CHAR_TMP
        TAD NEG_13
        SZA
        JMP CHECK_WS_LF
        JMP NN_LOOP
CHECK_WS_LF,
        CLA
        TAD CHAR_TMP
        TAD NEG_TEN
        SZA
        JMP NN_DONE
        JMP NN_LOOP
NN_DONE,
        CLA
        TAD CHAR_TMP
        JMP I NEXT_NONSPACE

/------------------------------------------------------------
/ NEXT_CHAR - Return next character, honoring buffered peek.
/------------------------------------------------------------
NEXT_CHAR, 0
        CLA
        TAD PEEK_CHAR
        SZA
        JMP HAVE_PEEK
        JMS I READ_CHAR_PTR
        JMP I NEXT_CHAR
HAVE_PEEK,
        DCA NEXT_TMP
        CLA
        DCA PEEK_CHAR
        TAD NEXT_TMP
        JMP I NEXT_CHAR

UNGET_CHAR, 0
        DCA PEEK_CHAR
        CLA
        JMP I UNGET_CHAR

/------------------------------------------------------------
/ READ_CHAR - Read a character from keyboard, echo to console.
/------------------------------------------------------------
READ_CHAR, 0
        JMS I GETCH_PTR
        DCA INCHAR
        CLA
        TAD INCHAR
        JMS I PUTCH_PTR
        CLA
        TAD INCHAR
        JMP I READ_CHAR

/------------------------------------------------------------
/ GETCH - Block until keyboard ready, return character in AC.
/------------------------------------------------------------
GETCH,  0
KWAIT,  IOT 6031
        JMP KWAIT
        IOT 6032
        JMP I GETCH

/------------------------------------------------------------
/ MUL10_ADD_DIGIT - NUMBER = NUMBER * 10 + DIGIT.
/------------------------------------------------------------
MUL10_ADD_DIGIT, 0
        CLA
        DCA NEWVAL
        CLA
        TAD NUMBER
        DCA OLDVAL
        CLA
        TAD NEG_TEN
        DCA MUL_COUNT
MUL10_LOOP,
        CLA
        TAD NEWVAL
        TAD OLDVAL
        DCA NEWVAL
        ISZ MUL_COUNT
        JMP MUL10_LOOP
        CLA
        TAD NEWVAL
        TAD DIGIT
        DCA NUMBER
        CLA
        JMP I MUL10_ADD_DIGIT

/------------------------------------------------------------
/ PRINT_DAY_FIELD - Print current day number with width three.
/------------------------------------------------------------
PRINT_DAY_FIELD, 0
        CLA
        TAD DAY_NUMBER
        DCA REM_DAY
        CLA
        TAD DAY_NUMBER
        TAD NEG_TEN
        SMA
        JMP TWO_DIGITS

SINGLE_DIGIT,
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        TAD REM_DAY
        TAD ASCII_ZERO
        JMS I PUTCH_PTR
        JMP DAY_TRAIL

TWO_DIGITS,
        CLA
        DCA TENS
TEN_LOOP,
        CLA
        TAD REM_DAY
        TAD NEG_TEN
        SMA
        JMP TEN_STORE
        TAD TEN
        JMP TEN_DONE
TEN_STORE,
        DCA REM_DAY
        ISZ TENS
        JMP TEN_LOOP
TEN_DONE,
        DCA REM_DAY
        CLA
        TAD TENS
        TAD ASCII_ZERO
        JMS I PUTCH_PTR
        CLA
        TAD REM_DAY
        TAD ASCII_ZERO
        JMS I PUTCH_PTR

DAY_TRAIL,
        CLA
        TAD SPACE
        JMS I PUTCH_PTR
        CLA
        JMP I PRINT_DAY_FIELD

/------------------------------------------------------------
/ PRINT_DECIMAL - Print value in AC as unsigned decimal.
/------------------------------------------------------------
PRINT_DECIMAL, 0
        DCA DEC_VALUE
        CLA
        DCA DEC_STARTED

        CLA
        DCA DIGIT
PD1000_LOOP,
        CLA
        TAD DEC_VALUE
        TAD DEC_NEG_1000
        SMA
        JMP PD1000_STORE
        TAD DEC_K1000
        JMP PD1000_DONE
PD1000_STORE,
        DCA DEC_VALUE
        ISZ DIGIT
        JMP PD1000_LOOP
PD1000_DONE,
        DCA DEC_VALUE
        CLA
        TAD DIGIT
        SZA
        JMP PD1000_PRINT
        CLA
        TAD DEC_STARTED
        SZA
        JMP PD100_SECTION
PD1000_PRINT,
        CLA
        TAD DIGIT
        TAD ASCII_ZERO
        JMS I PUTCH_PTR
        CLA
        TAD ONE
        DCA DEC_STARTED
PD100_SECTION,
        CLA
        DCA DIGIT
PD100_LOOP,
        CLA
        TAD DEC_VALUE
        TAD DEC_NEG_100
        SMA
        JMP PD100_STORE
        TAD DEC_K100
        JMP PD100_DONE
PD100_STORE,
        DCA DEC_VALUE
        ISZ DIGIT
        JMP PD100_LOOP
PD100_DONE,
        DCA DEC_VALUE
        CLA
        TAD DIGIT
        SZA
        JMP PD100_PRINT
        CLA
        TAD DEC_STARTED
        SZA
        JMP PD10_SECTION
PD100_PRINT,
        CLA
        TAD DIGIT
        TAD ASCII_ZERO
        JMS I PUTCH_PTR
        CLA
        TAD ONE
        DCA DEC_STARTED
PD10_SECTION,
        CLA
        DCA DIGIT
PD10_LOOP,
        CLA
        TAD DEC_VALUE
        TAD NEG_TEN
        SMA
        JMP PD10_STORE
        TAD TEN
        JMP PD10_DONE
PD10_STORE,
        DCA DEC_VALUE
        ISZ DIGIT
        JMP PD10_LOOP
PD10_DONE,
        DCA DEC_VALUE
        CLA
        TAD DIGIT
        SZA
        JMP PD10_PRINT
        CLA
        TAD DEC_STARTED
        SZA
        JMP PD1_PRINT
PD10_PRINT,
        CLA
        TAD DIGIT
        TAD ASCII_ZERO
        JMS I PUTCH_PTR
        CLA
        TAD ONE
        DCA DEC_STARTED
PD1_PRINT,
        CLA
        TAD DEC_VALUE
        TAD ASCII_ZERO
        JMS I PUTCH_PTR
        CLA
        JMP I PRINT_DECIMAL_PTR

DEC_K1000,    01750
DEC_NEG_1000, 06030
DEC_K100,     00144
DEC_NEG_100,  07634

/------------------------------------------------------------
/ PRINT - Null-terminated string print.
/------------------------------------------------------------
PRINT,  0
        DCA STR_PTR
PR_LOOP,
        TAD I STR_PTR
        SZA
        JMP PR_CONT
        JMP I PRINT
PR_CONT,
        JMS I PUTCH_PTR
        ISZ STR_PTR
        JMP PR_LOOP

/------------------------------------------------------------
/ PUTCH - Transmit character in AC on KL8E.
/------------------------------------------------------------
PUTCH,  0
        DCA OUTBUF
PWAIT,  IOT 6041
        JMP PWAIT
        TAD OUTBUF
        IOT 6046
        CLA
        JMP I PUTCH

/------------------------------------------------------------
/ NEWLINE - Emit CR/LF pair.
/------------------------------------------------------------
NEWLINE, 0
        CLA
        TAD CHAR_CR
        JMS I PUTCH_PTR
        CLA
        TAD CHAR_LF
        JMS I PUTCH_PTR
        CLA
        JMP I NEWLINE

/------------------------------------------------------------
/ CONSUME_EOL - Remove a pending newline if buffered.
/------------------------------------------------------------
CONSUME_EOL, 0
        CLA
        TAD PEEK_CHAR
        SZA
        JMP CE_CHECK
        CLA
        JMP I CONSUME_EOL_PTR
CE_CHECK,
        TAD NEG_TEN
        SZA
        JMP CE_CR
        CLA
        DCA PEEK_CHAR
        JMP I CONSUME_EOL_PTR
CE_CR,
        CLA
        TAD PEEK_CHAR
        TAD NEG_13
        SZA
        JMP CE_CLEAR
        CLA
        JMP I CONSUME_EOL_PTR
CE_CLEAR,
        CLA
        DCA PEEK_CHAR
        JMP I CONSUME_EOL_PTR

/------------------------------------------------------------
/ SKIP_LINE - Discard characters up to and including newline.
/------------------------------------------------------------
SKIP_LINE, 0
SKIP_LOOP,
        JMS I NEXT_CHAR_PTR
        DCA CHAR_TMP
        CLA
        TAD CHAR_TMP
        TAD NEG_TEN
        SZA
        JMP SKIP_CR
        JMP I SKIP_LINE_PTR
SKIP_CR,
        CLA
        TAD CHAR_TMP
        TAD NEG_13
        SZA
        JMP SKIP_LOOP
        JMP I SKIP_LINE_PTR

/------------------------------------------------------------
/ MOD400, MOD100, MOD7 helpers using shared REM_WORK.
/------------------------------------------------------------
MOD400, 0
        CLA
        TAD YEAR_WORK
        DCA REM_WORK
M400_LOOP,
        CLA
        TAD REM_WORK
        TAD M400_NEG
        SMA
        JMP M400_STORE
        TAD M400_K
        JMP M400_DONE
M400_STORE,
        DCA REM_WORK
        JMP M400_LOOP
M400_DONE,
        CLA
        TAD REM_WORK
        JMP I MOD400

M400_K,   00620
M400_NEG, 07160

MOD100, 0
        CLA
        TAD YEAR_WORK
        DCA REM_WORK
M100_LOOP,
        CLA
        TAD REM_WORK
        TAD M100_NEG
        SMA
        JMP M100_STORE
        TAD M100_K
        JMP M100_DONE
M100_STORE,
        DCA REM_WORK
        JMP M100_LOOP
M100_DONE,
        CLA
        TAD REM_WORK
        JMP I MOD100

M100_K,   00144
M100_NEG, 07634

MOD4,   0
        CLA
        TAD YEAR_WORK
        AND MOD4_MASK
        JMP I MOD4

MOD4_MASK, 0003

MOD7,   0
        CLA
        TAD REM_WORK
        DCA REM_WORK
M7_LOOP,
        CLA
        TAD REM_WORK
        TAD NEG_SEVEN
        SMA
        JMP M7_STORE
        TAD MOD7_K
        JMP M7_DONE
M7_STORE,
        DCA REM_WORK
        JMP M7_LOOP
M7_DONE,
        CLA
        TAD REM_WORK
        JMP I MOD7

MOD7_K, 0007

/------------------------------------------------------------
/ IS_LEAP - Return 1 if YEAR_WORK is leap year, else 0.
/------------------------------------------------------------
IS_LEAP,0
        JMS I MOD400_PTR
        SZA
        JMP CHECK_100
        CLA
        TAD ONE
        JMP I IS_LEAP
CHECK_100,
        JMS I MOD100_PTR
        SZA
        JMP CHECK_4
        CLA
        JMP I IS_LEAP
CHECK_4,
        JMS I MOD4_PTR
        SZA
        JMP NOT_LEAP
        CLA
        TAD ONE
        JMP I IS_LEAP
NOT_LEAP,
        CLA
        JMP I IS_LEAP

/------------------------------------------------------------
/ DATA SECTION
/------------------------------------------------------------

WEEK_HDR, "S";"u";" ";"M";"o";" ";"T";"u";" ";"W";"e";" ";"T";"h";" ";"F";"r";" ";"S";"a";0000
ERR_INPUT,"E";"x";"p";"e";"c";"t";"e";"d";" ";"d";"e";"c";"i";"m";"a";"l";" ";"n";"u";"m";"b";"e";"r";".";0015;0012;0000
ERR_YEAR, "Y";"e";"a";"r";" ";"m";"u";"s";"t";" ";"b";"e";" ";"1";"9";"5";"7";" ";"o";"r";" ";"l";"a";"t";"e";"r";".";0015;0012;0000
ERR_MONTH,"M";"o";"n";"t";"h";" ";"m";"u";"s";"t";" ";"b";"e";" ";"1";"-";"1";"2";".";0015;0012;0000

MONTH_NAME_PTRS,
        MONTH_JAN
        MONTH_FEB
        MONTH_MAR
        MONTH_APR
        MONTH_MAY
        MONTH_JUN
        MONTH_JUL
        MONTH_AUG
        MONTH_SEP
        MONTH_OCT
        MONTH_NOV
        MONTH_DEC

MONTH_JAN, "J";"a";"n";"u";"a";"r";"y";0000
MONTH_FEB, "F";"e";"b";"r";"u";"a";"r";"y";0000
MONTH_MAR, "M";"a";"r";"c";"h";0000
MONTH_APR, "A";"p";"r";"i";"l";0000
MONTH_MAY, "M";"a";"y";0000
MONTH_JUN, "J";"u";"n";"e";0000
MONTH_JUL, "J";"u";"l";"y";0000
MONTH_AUG, "A";"u";"g";"u";"s";"t";0000
MONTH_SEP, "S";"e";"p";"t";"e";"m";"b";"e";"r";0000
MONTH_OCT, "O";"c";"t";"o";"b";"e";"r";0000
MONTH_NOV, "N";"o";"v";"e";"m";"b";"e";"r";0000
MONTH_DEC, "D";"e";"c";"e";"m";"b";"e";"r";0000

MONTH_LENGTHS,
        037
        034
        037
        036
        037
        036
        037
        037
        036
        037
        036
        037

        $                       / End of program
