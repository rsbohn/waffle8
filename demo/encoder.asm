/ encoder.asm — Streaming SIXBIT encoder at 0600
/ Reads keyboard input, tokenizes on whitespace, and emits PDP-8 plaintext
/ tokens as octal words (two SIXBIT characters per 12-bit word).
/ State machine:
/   - First token must be a category (INVENTORY/RECEIVING/WAREHOUSE/KITCHEN).
/   - A fixed date marker follows automatically.
/   - Remaining tokens are treated as items followed by a fixed quantity marker.

        *0
STATE,          0
STOP_FLAG,      0
TOKEN_LEN,      0
TOKEN_PTR,      0
TOKEN_READ_PTR, 0
TMP_PTR,        0
COUNTER,        0
CHAR,           0
LETTER_TEMP,    0
LETTER_A,       0
LETTER_B,       0
REMAIN,         0
WORD0,          0
WORD1,          0
WORD_COUNT,     0
WORD_TEMP,      0
OCT_WORK,       0
OCT_DIGIT,      0
CHAR_TEMP,      0
CMP_WORD0,      0
CMP_WORD1,      0
CAT_PTR,        0
EXPECT0,        0
EXPECT1,        0
MSG_PTR,        0
TOKEN_BASE,     TOKEN_BUF
CAT_TABLE_PTR,  CAT_TABLE
ERR_PTR,        ERR_MSG

READ_CHAR,      0
        JMP I READ_CHAR_PTR
READ_CHAR_PTR,  READ_CHAR_BODY

HANDLE_CHAR,    0
        JMP I HANDLE_CHAR_PTR
HANDLE_CHAR_PTR, HANDLE_CHAR_BODY

ADD_LETTER,     0
        JMP I ADD_LETTER_PTR
ADD_LETTER_PTR, ADD_LETTER_BODY

FINALIZE_TOKEN, 0
        JMP I FINALIZE_TOKEN_PTR
FINALIZE_TOKEN_PTR, FINALIZE_TOKEN_BODY

CLEAR_TOKEN_BUF, 0
        JMP I CLEAR_TOKEN_BUF_PTR
CLEAR_TOKEN_BUF_PTR, CLEAR_TOKEN_BUF_BODY

PROCESS_TOKEN,  0
        JMP I PROCESS_TOKEN_PTR
PROCESS_TOKEN_PTR, PROCESS_TOKEN_BODY

PACK_TOKEN,     0
        JMP I PACK_TOKEN_PTR
PACK_TOKEN_PTR, PACK_TOKEN_BODY

LOAD_LETTER,    0
        JMP I LOAD_LETTER_PTR
LOAD_LETTER_PTR, LOAD_LETTER_BODY

BUILD_WORD,     0
        JMP I BUILD_WORD_PTR
BUILD_WORD_PTR, BUILD_WORD_BODY

EMIT_PACKED,    0
        JMP I EMIT_PACKED_PTR
EMIT_PACKED_PTR, EMIT_PACKED_BODY

EMIT_DATE_MARKER, 0
        JMP I EMIT_DATE_PTR
EMIT_DATE_PTR,  EMIT_DATE_MARKER_BODY

EMIT_NUMBER_MARKER, 0
        JMP I EMIT_NUMBER_PTR
EMIT_NUMBER_PTR, EMIT_NUMBER_MARKER_BODY

VALIDATE_CATEGORY, 0
        JMP I VALIDATE_CATEGORY_PTR
VALIDATE_CATEGORY_PTR, VALIDATE_CATEGORY_BODY

PRINT_WORD,     0
        JMP I PRINT_WORD_PTR
PRINT_WORD_PTR, PRINT_WORD_BODY

PRINT_CRLF,     0
        JMP I PRINT_CRLF_PTR
PRINT_CRLF_PTR, PRINT_CRLF_BODY

PRINT_CATEGORY_ERROR, 0
        JMP I PRINT_CATEGORY_ERROR_PTR
PRINT_CATEGORY_ERROR_PTR, PRINT_CATEGORY_ERROR_BODY

PUTCHR,         0
        JMP I PUTCHR_PTR
PUTCHR_PTR,     PUTCHR_BODY
DATE_MARKER,    07070
NUMBER_MARKER,  07071
DATE_YEAR_WORD, 03751          / 2025
DATE_MDAY_WORD, 00537          / 10*32 + 31
QUANTITY_WORD,  00220          / 144 decimal
OCT_MASK0,      07000
OCT_MASK1,      00700
OCT_MASK2,      00070
OCT_MASK3,      00007
NEG1000,        07000
NEG0100,        07700
NEG0010,        07770
DIGIT_BASE,     060
CHAR_CR,        015
CHAR_LF,        012
ONE,            0001
NEG1,           07777
NEG2,           07776
NEG4,           07774
NEG_A,          07677
NEG_Z,          07646
NEG_LOWER_A,    07637
NEG_LOWER_Z,    07606
NEG_040,        07740
NEG_CR,         07763
NEG_LF,         07766
NEG_0100,       07700

        *0600
START,  CLA CLL
        DCA STATE
        DCA STOP_FLAG
        JMS CLEAR_TOKEN_BUF

MAIN_LOOP,
        JMS READ_CHAR
        JMS HANDLE_CHAR
        CLA
        TAD STOP_FLAG
        SZA
        JMP HALT_DONE
        JMP MAIN_LOOP

HALT_DONE,
        HLT
        JMP HALT_DONE

/------------------------------------------------------------------------------
/ Input handling
/------------------------------------------------------------------------------
        *1000
READ_CHAR_BODY,
KC_WAIT,
        IOT 6031                / Wait for keyboard character
        JMP KC_WAIT
        IOT 6036                / Read character into AC
        DCA CHAR
        JMP I READ_CHAR

        *1200
HANDLE_CHAR_BODY,
        / Convert lowercase to uppercase when needed.
        CLA
        TAD CHAR
        TAD NEG_LOWER_A
        SMA                     / < 'a'? skip conversion
        JMP CHECK_UPPER_RANGE
        CLA
        TAD CHAR
        TAD NEG_LOWER_Z
        SPA                     / > 'z'? skip conversion
        JMP CHECK_UPPER_RANGE
        CLA
        TAD CHAR
        TAD NEG_040             / subtract 040 to map lowercase -> uppercase
        DCA CHAR

CHECK_UPPER_RANGE,
        CLA
        TAD CHAR
        TAD NEG_A
        SMA
        JMP NOT_ALPHA
        CLA
        TAD CHAR
        TAD NEG_Z
        SPA
        JMP NOT_ALPHA
        JMS ADD_LETTER
        JMP I HANDLE_CHAR

NOT_ALPHA,
        JMS FINALIZE_TOKEN
        CLA
        TAD CHAR
        TAD NEG_CR
        SZA                     / CR?
        JMP CHECK_LF
        CLA
        TAD ONE
        DCA STOP_FLAG
        JMP I HANDLE_CHAR

CHECK_LF,
        CLA
        TAD CHAR
        TAD NEG_LF
        SZA
        JMP I HANDLE_CHAR
        CLA
        TAD ONE
        DCA STOP_FLAG
        JMP I HANDLE_CHAR

        *1400
/------------------------------------------------------------------------------
/ Token buffer helpers
/------------------------------------------------------------------------------
ADD_LETTER_BODY,
        CLA
        TAD TOKEN_LEN
        TAD NEG4
        SPA                     / Already holding four characters
        JMP I ADD_LETTER
        CLA
        TAD CHAR
        TAD NEG_0100
        DCA LETTER_TEMP
        CLA
        TAD LETTER_TEMP
        DCA I TOKEN_PTR
        ISZ TOKEN_PTR
        CLA
        TAD TOKEN_LEN
        TAD ONE
        DCA TOKEN_LEN
        JMP I ADD_LETTER
        *1600
FINALIZE_TOKEN_BODY,
        CLA
        TAD TOKEN_LEN
        SZA
        JMP DO_PROCESS
        JMS CLEAR_TOKEN_BUF
        JMP I FINALIZE_TOKEN
DO_PROCESS,
        JMS PROCESS_TOKEN
        JMS CLEAR_TOKEN_BUF
        JMP I FINALIZE_TOKEN
        *2000
CLEAR_TOKEN_BUF_BODY,
        CLA
        TAD TOKEN_BASE
        DCA TMP_PTR
        CLA
        DCA TOKEN_LEN
        CLA
        TAD NEG4
        DCA COUNTER
CLR_LOOP,
        CLA
        DCA I TMP_PTR
        ISZ TMP_PTR
        ISZ COUNTER
        JMP CLR_LOOP
        CLA
        TAD TOKEN_BASE
        DCA TOKEN_PTR
        JMP I CLEAR_TOKEN_BUF

/------------------------------------------------------------------------------
/ Token processing and emission
/------------------------------------------------------------------------------
        *2200
PROCESS_TOKEN_BODY,
        JMS PACK_TOKEN
        CLA
        TAD STATE
        SZA
        JMP TOKEN_ITEM
        JMS VALIDATE_CATEGORY
        JMS EMIT_PACKED
        JMS EMIT_DATE_MARKER
        CLA
        TAD ONE
        DCA STATE
        JMP I PROCESS_TOKEN

TOKEN_ITEM,
        JMS EMIT_PACKED
        JMS EMIT_NUMBER_MARKER
        JMP I PROCESS_TOKEN
        *2400
PACK_TOKEN_BODY,
        CLA
        TAD TOKEN_LEN
        DCA REMAIN
        CLA
        TAD TOKEN_BASE
        DCA TOKEN_READ_PTR
        CLA
        DCA WORD1

        JMS LOAD_LETTER
        DCA LETTER_A
        JMS LOAD_LETTER
        DCA LETTER_B
        JMS BUILD_WORD
        DCA WORD0
        CLA
        TAD ONE
        DCA WORD_COUNT

        CLA
        TAD REMAIN
        SZA
        JMP PACK_DONE
        JMS LOAD_LETTER
        DCA LETTER_A
        JMS LOAD_LETTER
        DCA LETTER_B
        JMS BUILD_WORD
        DCA WORD1
        CLA
        TAD WORD_COUNT
        TAD ONE
        DCA WORD_COUNT
PACK_DONE,
        JMP I PACK_TOKEN
        *2600
LOAD_LETTER_BODY,
        CLA
        TAD REMAIN
        SZA
        JMP LOAD_HAS
        CLA
        DCA LETTER_TEMP
        CLA
        JMP I LOAD_LETTER
LOAD_HAS,
        CLA
        TAD I TOKEN_READ_PTR
        DCA LETTER_TEMP
        ISZ TOKEN_READ_PTR
        CLA
        TAD REMAIN
        TAD NEG1
        DCA REMAIN
        CLA
        TAD LETTER_TEMP
        JMP I LOAD_LETTER
        *3000
BUILD_WORD_BODY,
        CLA CLL
        TAD LETTER_A
        RAL
        RAL
        RAL
        RAL
        RAL
        RAL
        TAD LETTER_B
        JMP I BUILD_WORD
        *3200
EMIT_PACKED_BODY,
        CLA
        TAD WORD0
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        CLA
        TAD WORD_COUNT
        TAD NEG2
        SPA
        JMP I EMIT_PACKED
        CLA
        TAD WORD1
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        JMP I EMIT_PACKED
        *3400
EMIT_DATE_MARKER_BODY,
        CLA
        TAD DATE_MARKER
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        CLA
        TAD DATE_YEAR_WORD
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        CLA
        TAD DATE_MDAY_WORD
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        JMP I EMIT_DATE_MARKER

        *3600
EMIT_NUMBER_MARKER_BODY,
        CLA
        TAD NUMBER_MARKER
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        CLA
        TAD QUANTITY_WORD
        DCA WORD_TEMP
        JMS PRINT_WORD
        JMS PRINT_CRLF
        JMP I EMIT_NUMBER_MARKER
        *4000
VALIDATE_CATEGORY_BODY,
        CLA
        TAD WORD0
        DCA CMP_WORD0
        CLA
        TAD WORD1
        DCA CMP_WORD1
        CLA
        TAD CAT_TABLE_PTR
        DCA CAT_PTR
VAL_LOOP,
        CLA
        TAD I CAT_PTR
        SZA
        JMP HAVE_ENTRY
        JMS PRINT_CATEGORY_ERROR
        HLT
HAVE_ENTRY,
        DCA EXPECT0
        ISZ CAT_PTR
        CLA
        TAD I CAT_PTR
        DCA EXPECT1
        ISZ CAT_PTR
        CLA
        TAD EXPECT0
        CMA
        IAC
        TAD CMP_WORD0
        SZA
        JMP VAL_LOOP
        CLA
        TAD EXPECT1
        CMA
        IAC
        TAD CMP_WORD1
        SZA
        JMP VAL_LOOP
        CLA
        JMP I VALIDATE_CATEGORY

/------------------------------------------------------------------------------
/ Output helpers
/------------------------------------------------------------------------------
        *4200
PRINT_WORD_BODY,
        CLA
        TAD WORD_TEMP
        AND OCT_MASK0
        DCA OCT_WORK
        CLA
        DCA OCT_DIGIT
PW_THOUSANDS,
        CLA
        TAD OCT_WORK
        TAD NEG1000
        SMA
        JMP PW_THOUSANDS_DONE
        DCA OCT_WORK
        ISZ OCT_DIGIT
        JMP PW_THOUSANDS
PW_THOUSANDS_DONE,
        CLA
        TAD OCT_DIGIT
        TAD DIGIT_BASE
        JMS PUTCHR

        CLA
        TAD WORD_TEMP
        AND OCT_MASK1
        DCA OCT_WORK
        CLA
        DCA OCT_DIGIT
PW_HUNDREDS,
        CLA
        TAD OCT_WORK
        TAD NEG0100
        SMA
        JMP PW_HUNDREDS_DONE
        DCA OCT_WORK
        ISZ OCT_DIGIT
        JMP PW_HUNDREDS
PW_HUNDREDS_DONE,
        CLA
        TAD OCT_DIGIT
        TAD DIGIT_BASE
        JMS PUTCHR

        CLA
        TAD WORD_TEMP
        AND OCT_MASK2
        DCA OCT_WORK
        CLA
        DCA OCT_DIGIT
PW_TENS,
        CLA
        TAD OCT_WORK
        TAD NEG0010
        SMA
        JMP PW_TENS_DONE
        DCA OCT_WORK
        ISZ OCT_DIGIT
        JMP PW_TENS
PW_TENS_DONE,
        CLA
        TAD OCT_DIGIT
        TAD DIGIT_BASE
        JMS PUTCHR

        CLA
        TAD WORD_TEMP
        AND OCT_MASK3
        TAD DIGIT_BASE
        JMS PUTCHR
        JMP I PRINT_WORD
        *4400
PRINT_CRLF_BODY,
        CLA
        TAD CHAR_CR
        JMS PUTCHR
        CLA
        TAD CHAR_LF
        JMS PUTCHR
        JMP I PRINT_CRLF
        *4600
PUTCHR_BODY,
        DCA CHAR_TEMP
TTY_WAIT,
        IOT 6041                / Wait for teleprinter ready
        JMP TTY_WAIT
        CLA
        TAD CHAR_TEMP
        IOT 6046                / Transmit
        JMP I PUTCHR
        *5000
PRINT_CATEGORY_ERROR_BODY,
        CLA
        TAD ERR_PTR
        DCA MSG_PTR
ERR_LOOP,
        CLA
        TAD I MSG_PTR
        SZA
        JMP ERR_DONE
        JMS PUTCHR
        ISZ MSG_PTR
        JMP ERR_LOOP
ERR_DONE,
        JMP I PRINT_CATEGORY_ERROR

/------------------------------------------------------------------------------
/ Persistent storage outside page zero
/------------------------------------------------------------------------------
TOKEN_BUF,
        0
        0
        0
        0

/ Category table: word0, word1 pairs, terminated by 0000
CAT_TABLE,
        01116
        02605
        02205
        00305
        02701
        02205
        01311
        02403
        0000

ERR_MSG,
        "B";"A";"D";" "; "C";"A";"T";"E";"G";"O";"R";"Y";015;012;0000
