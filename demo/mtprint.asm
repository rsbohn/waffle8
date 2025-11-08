/ mtprint.asm
/ Print the header and payload of the current magtape record.
/ 
/ The program reads the desired magtape unit from zero-page location 0007,
/ selects that unit, decodes the six-word header (label + data format), prints
/ both fields, then emits the payload. ASCII records are streamed as 8-bit
/ characters; SIXBIT records are expanded using the standard DEC character map.
/ Output is sent to the line printer device (660x IOTs).

        *0010                   / Auto-increment pointers
HDR_PTR,    0
LBL_PTR,    0
FMT_PTR,    0
STR_PTR,    0
DST_PTR,    0
CMP_PTR,    0

        *0020                   / Zero-page workspace and constants
NEG6,       07772             / -6
NEG3,       07775             / -3
NEG2,       07776             / -2
NEG1,       07777             / -1
NEG8,       07770             / -8
NEG30,      07750             / -30
NEG31,      07747             / -31
NEG32,      07746             / -32
NEG50,      07730             / -50
NEG1000,    07000             / -01000
NEG0100,    07700             / -0100
NEG0010,    07770             / -0010
UNIT_MASK,  0007
OCT_MASK0,  07000
OCT_MASK1,  00700
OCT_MASK2,  00070
OCT_MASK3,  00007
ASCII_MASK, 0177
HEADER_BAD, 0
FORMAT_KIND,0
UNIT_NUM,   0
WORDS_LEFT, 0
SIXMASK,    0077
CR,         0015
LF,         0012
SPACE,      0040
LBRACKET,   0133
RBRACKET,   0135
DIGIT_BASE, 0060

HEADER_ADDR,        HEADER
LABEL_ADDR,         LABEL_BUF
FORMAT_ADDR,        FORMAT_BUF
ASCII_TAG_ADDR,     ASCII_TAG
BINARY_TAG_ADDR,    BINARY_TAG
SIXBIT_TAG_ADDR,    SIXBIT_TAG
ASCII_DESC_ADDR,    ASCII_DESC
BINARY_DESC_ADDR,   BINARY_DESC
SIXBIT_DESC_ADDR,   SIXBIT_DESC
UNKNOWN_DESC_ADDR,  UNKNOWN_DESC
MAG_PREFIX_PTR,     MAG_PREFIX
PAYLOAD_MSG_PTR,    PAYLOAD_MSG
UNKNOWN_MSG_PTR,    UNKNOWN_MSG
BAD_HEADER_MSG_PTR, BAD_HEADER_MSG
READWORD_PTR,       READWORD
READ_HDR_PTR,       READ_HDR
CHARMAP_PTR,        CHARMAP
STORE_PAIR_PTR,     STORE_PAIR
SHIFT_RIGHT6_PTR,   SHIFT_RIGHT6
PRINT_WORD_OCT_PTR, PRINT_WORD_OCT
PRINT_STR_PTR,      PRINT_STR
PRINT_FIXED_PTR,    PRINT_FIXED
PRINT_CRLF_PTR,     PRINT_CRLF
PRINT_DESC_ASCII_PTR,   PRINT_DESC_ASCII
PRINT_DESC_BINARY_PTR,  PRINT_DESC_BINARY
PRINT_DESC_SIXBIT_PTR,  PRINT_DESC_SIXBIT
PRINT_DESC_UNKNOWN_PTR, PRINT_DESC_UNKNOWN
PRINT_DESC_CLOSE_PTR,   PRINT_DESC_CLOSE
SIX_TO_ASCII_PTR,   SIX_TO_ASCII
PUTCHR_PTR,         PUTCHR
COMPARE6_PTR,       COMPARE6
BAD_HEADER_PTR,     BAD_HEADER
ASCII_PAYLOAD_PTR,  ASCII_PAYLOAD
BINARY_PAYLOAD_PTR, BINARY_PAYLOAD
SIXBIT_PAYLOAD_PTR, SIXBIT_PAYLOAD
VALIDATE_CODE_PTR,  VALIDATE_CODE

WORD_TEMP,  0
CHAR_TEMP,  0
SIX_INDEX,  0
TAB_PTR,    0
COUNT6,     0
BIN_WORD_COUNT, 0
OCT_WORK,   0
OCT_DIGIT,  0

        *0200
START,  CLA CLL
        TAD 0007                / Load requested unit from zero-page
        AND UNIT_MASK
        DCA UNIT_NUM

        CLA CLL
        TAD UNIT_NUM
        IOT 6701                / Select magtape unit (GO)

        CLA
        DCA HEADER_BAD
        DCA FORMAT_KIND

        CLA CLL
        TAD HEADER_ADDR
        TAD NEG1
        DCA HDR_PTR
        CLA CLL
        TAD NEG6
        DCA WORDS_LEFT
        JMS I READ_HDR_PTR

        / Decode label into LABEL_BUF
        CLA CLL
        TAD HEADER_ADDR
        TAD NEG1
        DCA HDR_PTR
        CLA CLL
        TAD LABEL_ADDR
        TAD NEG1
        DCA LBL_PTR
        CLA
        TAD LBL_PTR
        DCA DST_PTR

        CLA CLL
        TAD NEG3
        DCA WORDS_LEFT

DECODE_LABEL,
        CLA CLL
        TAD I HDR_PTR
        JMS I STORE_PAIR_PTR
        CLA
        TAD DST_PTR
        DCA LBL_PTR
        ISZ WORDS_LEFT
        JMP DECODE_LABEL

        / Decode format into FORMAT_BUF
        CLA CLL
        TAD FORMAT_ADDR
        TAD NEG1
        DCA FMT_PTR
        CLA
        TAD FMT_PTR
        DCA DST_PTR

        CLA CLL
        TAD NEG3
        DCA WORDS_LEFT

DECODE_FORMAT,
        CLA CLL
        TAD I HDR_PTR
        JMS I STORE_PAIR_PTR
        CLA
        TAD DST_PTR
        DCA FMT_PTR
        ISZ WORDS_LEFT
        JMP DECODE_FORMAT

        / Determine data interpretation based on FORMAT_BUF
        CLA
        DCA FORMAT_KIND

        CLA CLL
        TAD FORMAT_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD ASCII_TAG_ADDR
        TAD NEG1
        DCA CMP_PTR
        JMS I COMPARE6_PTR
        SZA
        JMP CHECK_BINARY
        CLA
        IAC                     / FORMAT_KIND = 1 (ASCII)
        DCA FORMAT_KIND
        JMP FORMAT_CHOICE_DONE

CHECK_BINARY,
        CLA CLL
        TAD FORMAT_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD BINARY_TAG_ADDR
        TAD NEG1
        DCA CMP_PTR
        JMS I COMPARE6_PTR
        SZA
        JMP CHECK_SIXBIT
        CLA
        IAC
        IAC
        IAC                     / FORMAT_KIND = 3 (BINARY)
        DCA FORMAT_KIND
        JMP FORMAT_CHOICE_DONE

CHECK_SIXBIT,
        CLA CLL
        TAD FORMAT_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD SIXBIT_TAG_ADDR
        TAD NEG1
        DCA CMP_PTR
        JMS I COMPARE6_PTR
        SZA
        JMP FORMAT_CHOICE_DONE
        CLA
        IAC
        IAC                     / FORMAT_KIND = 2 (SIXBIT)
        DCA FORMAT_KIND

FORMAT_CHOICE_DONE,
        CLA
        TAD HEADER_BAD
        SZA
        JMP I BAD_HEADER_PTR

        / Print header information as "MAG<unit> <label> [<type>]"
        CLA CLL
        TAD MAG_PREFIX_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR

        CLA
        TAD UNIT_NUM
        TAD DIGIT_BASE
        JMS I PUTCHR_PTR

        CLA
        TAD SPACE
        JMS I PUTCHR_PTR

        CLA CLL
        TAD LABEL_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD NEG6
        DCA COUNT6
        JMS I PRINT_FIXED_PTR

        CLA
        TAD SPACE
        JMS I PUTCHR_PTR

        CLA
        TAD LBRACKET
        JMS I PUTCHR_PTR

        CLA
        TAD FORMAT_KIND
        SZA
        JMP SELECT_ASCII_DESC
        JMS I PRINT_DESC_UNKNOWN_PTR
        JMP I PRINT_DESC_CLOSE_PTR

SELECT_ASCII_DESC,
        CLA
        TAD FORMAT_KIND
        TAD NEG1
        SZA
        JMP SELECT_BINARY_DESC
        JMS I PRINT_DESC_ASCII_PTR
        JMP I PRINT_DESC_CLOSE_PTR

SELECT_BINARY_DESC,
        CLA
        TAD FORMAT_KIND
        TAD NEG3
        SZA
        JMP SELECT_SIXBIT_DESC
        JMS I PRINT_DESC_BINARY_PTR
        JMP I PRINT_DESC_CLOSE_PTR

SELECT_SIXBIT_DESC,
        CLA
        TAD FORMAT_KIND
        TAD NEG2
        SZA
        JMP SELECT_DESC_FALLBACK
        JMS I PRINT_DESC_SIXBIT_PTR
        JMP I PRINT_DESC_CLOSE_PTR

SELECT_DESC_FALLBACK,
        JMS I PRINT_DESC_UNKNOWN_PTR
        JMP I PRINT_DESC_CLOSE_PTR

PRINT_DESC_ASCII,
        0
        CLA CLL
        TAD ASCII_DESC_ADDR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMP I PRINT_DESC_ASCII

PRINT_DESC_BINARY,
        0
        CLA CLL
        TAD BINARY_DESC_ADDR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMP I PRINT_DESC_BINARY

PRINT_DESC_SIXBIT,
        0
        CLA CLL
        TAD SIXBIT_DESC_ADDR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMP I PRINT_DESC_SIXBIT

PRINT_DESC_UNKNOWN,
        0
        CLA CLL
        TAD UNKNOWN_DESC_ADDR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMP I PRINT_DESC_UNKNOWN

PRINT_DESC_CLOSE,
        CLA
        TAD RBRACKET
        JMS I PUTCHR_PTR
        JMS I PRINT_CRLF_PTR

        CLA CLL
        TAD PAYLOAD_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR

        CLA
        TAD FORMAT_KIND
        SZA
        JMP FORMAT_DISPATCH
        JMP UNKNOWN_FORMAT_MSG

FORMAT_DISPATCH,
        CLA
        TAD FORMAT_KIND
        TAD NEG1
        SZA
        JMP TRY_BINARY
        JMS I ASCII_PAYLOAD_PTR
        JMS I PRINT_CRLF_PTR
        JMP DONE

TRY_BINARY,
        CLA
        TAD FORMAT_KIND
        TAD NEG3
        SZA
        JMP TRY_SIXBIT
        JMS I BINARY_PAYLOAD_PTR
        JMS I PRINT_CRLF_PTR
        JMP DONE

TRY_SIXBIT,
        CLA
        TAD FORMAT_KIND
        TAD NEG2
        SZA
        JMP UNKNOWN_FORMAT_MSG
        JMS I SIXBIT_PAYLOAD_PTR
        JMS I PRINT_CRLF_PTR
        JMP DONE

UNKNOWN_FORMAT_MSG,
        CLA CLL
        TAD UNKNOWN_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR
        JMP DONE

BAD_HEADER,
        CLA CLL
        TAD BAD_HEADER_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR
        JMP DONE

DONE,   HLT

/------------------------------------------------------------
/ Subroutines
/------------------------------------------------------------

        *1600
READ_HDR,
        0
RH_LOOP,
        IOT 6710                / Skip if word ready
        JMP RH_ERROR            / No more data - bad header
        IOT 6702                / Read word directly
        DCA I HDR_PTR
        ISZ WORDS_LEFT
        JMP RH_LOOP
        JMP I READ_HDR
RH_ERROR,
        ISZ HEADER_BAD          / Mark header as bad
        JMP I READ_HDR

        *1700
READWORD,
        0
RD_WAIT,
        IOT 6710                / Skip when word is ready
        JMP RD_WAIT
        IOT 6702                / Read word into AC
        JMP I READWORD

        *1720
STORE_PAIR,
        0
        DCA WORD_TEMP
        CLA
        TAD WORD_TEMP
        JMS I SHIFT_RIGHT6_PTR
        JMS I VALIDATE_CODE_PTR
        JMS I SIX_TO_ASCII_PTR
        DCA CHAR_TEMP
        CLA
        TAD CHAR_TEMP
        DCA I DST_PTR
        CLA
        TAD WORD_TEMP
        AND SIXMASK
        JMS I VALIDATE_CODE_PTR
        JMS I SIX_TO_ASCII_PTR
        DCA CHAR_TEMP
        CLA
        TAD CHAR_TEMP
        DCA I DST_PTR
        JMP I STORE_PAIR

        *1745
SHIFT_RIGHT6,
        0
        CLL
        RAR
        CLL
        RAR
        CLL
        RAR
        CLL
        RAR
        CLL
        RAR
        CLL
        RAR
        AND SIXMASK
        JMP I SHIFT_RIGHT6

        *0600
PRINT_WORD_OCT,
        0
        CLA
        TAD WORD_TEMP
        AND OCT_MASK0
        DCA OCT_WORK
        CLA
        DCA OCT_DIGIT
PWO_TOP_LOOP,
        CLA
        TAD OCT_WORK
        TAD NEG1000
        SMA
        JMP PWO_TOP_DONE
        DCA OCT_WORK
        ISZ OCT_DIGIT
        JMP PWO_TOP_LOOP
PWO_TOP_DONE,
        CLA
        TAD OCT_DIGIT
        TAD DIGIT_BASE
        JMS I PUTCHR_PTR

        CLA
        TAD WORD_TEMP
        AND OCT_MASK1
        DCA OCT_WORK
        CLA
        DCA OCT_DIGIT
PWO_SECOND_LOOP,
        CLA
        TAD OCT_WORK
        TAD NEG0100
        SMA
        JMP PWO_SECOND_DONE
        DCA OCT_WORK
        ISZ OCT_DIGIT
        JMP PWO_SECOND_LOOP
PWO_SECOND_DONE,
        CLA
        TAD OCT_DIGIT
        TAD DIGIT_BASE
        JMS I PUTCHR_PTR

        CLA
        TAD WORD_TEMP
        AND OCT_MASK2
        DCA OCT_WORK
        CLA
        DCA OCT_DIGIT
PWO_THIRD_LOOP,
        CLA
        TAD OCT_WORK
        TAD NEG0010
        SMA
        JMP PWO_THIRD_DONE
        DCA OCT_WORK
        ISZ OCT_DIGIT
        JMP PWO_THIRD_LOOP
PWO_THIRD_DONE,
        CLA
        TAD OCT_DIGIT
        TAD DIGIT_BASE
        JMS I PUTCHR_PTR

        CLA
        TAD WORD_TEMP
        AND OCT_MASK3
        TAD DIGIT_BASE
        JMS I PUTCHR_PTR
        JMP I PRINT_WORD_OCT

        *1100
SIX_TO_ASCII,
        0
        DCA SIX_INDEX
        CLA
        TAD CHARMAP_PTR
        DCA TAB_PTR
        CLA
        TAD SIX_INDEX
        TAD TAB_PTR
        DCA TAB_PTR
        CLA
        TAD I TAB_PTR
        JMP I SIX_TO_ASCII

PRINT_STR,
        0
PS_LOOP,
        CLA
        TAD I STR_PTR
        SZA
        JMP PS_CONT
        JMP I PRINT_STR
PS_CONT,
        JMS I PUTCHR_PTR
        JMP PS_LOOP

PRINT_FIXED,
        0
PF_LOOP,
        CLA
        TAD I STR_PTR
        JMS I PUTCHR_PTR
        ISZ COUNT6
        JMP PF_LOOP
        JMP I PRINT_FIXED

PRINT_CRLF,
        0
        CLA
        TAD CR
        JMS I PUTCHR_PTR
        CLA
        TAD LF
        JMS I PUTCHR_PTR
        JMP I PRINT_CRLF

PUTCHR,
        0
        DCA CHAR_TEMP
LP_WAIT,
        IOT 6601                / Skip if line printer ready
        JMP LP_WAIT
        CLA
        TAD CHAR_TEMP
        IOT 6606                / Clear flag and print character
        JMP I PUTCHR

        *1200
COMPARE6,
        0
        CLA CLL
        TAD NEG6
        DCA COUNT6
CMP6_LOOP,
        CLA
        TAD I STR_PTR
        DCA CHAR_TEMP
        CLA
        TAD I CMP_PTR
        DCA WORD_TEMP
        CLA
        TAD CHAR_TEMP
        CMA
        IAC
        TAD WORD_TEMP
        SZA
        JMP CMP6_NE
        ISZ COUNT6
        JMP CMP6_LOOP
        CLA
        JMP I COMPARE6

CMP6_NE,
        CLA
        IAC
        JMP I COMPARE6

ASCII_PAYLOAD,
        0
ASCII_LOOP,
        IOT 6710                / Skip if word ready
        JMP ASCII_DONE
        IOT 6702                / Read word directly
        AND ASCII_MASK
        DCA CHAR_TEMP
        CLA
        TAD CHAR_TEMP
        JMS I PUTCHR_PTR
        JMP ASCII_LOOP
ASCII_DONE,
        JMP I ASCII_PAYLOAD

BINARY_PAYLOAD,
        0
        CLA
        DCA BIN_WORD_COUNT
BINARY_LOOP,
        IOT 6710
        JMP BINARY_DONE
        IOT 6702
        DCA WORD_TEMP

        CLA
        TAD BIN_WORD_COUNT
        SZA
        JMP BP_NEED_SPACE
        JMP BP_PRINT_WORD
BP_NEED_SPACE,
        CLA
        TAD SPACE
        JMS I PUTCHR_PTR
BP_PRINT_WORD,
        JMS I PRINT_WORD_OCT_PTR

        ISZ BIN_WORD_COUNT
        CLA
        TAD BIN_WORD_COUNT
        TAD NEG8
        SZA
        JMP BINARY_LOOP
        JMS I PRINT_CRLF_PTR
        CLA
        DCA BIN_WORD_COUNT
        JMP BINARY_LOOP

BINARY_DONE,
        CLA
        TAD BIN_WORD_COUNT
        SZA
        JMP BP_NO_TRAIL
        JMS I PRINT_CRLF_PTR
BP_NO_TRAIL,
        JMP I BINARY_PAYLOAD

SIXBIT_PAYLOAD,
        0
SIX_LOOP,
        IOT 6710                / Skip if word ready
        JMP SIX_DONE
        IOT 6702                / Read word directly
        DCA WORD_TEMP
        CLA
        TAD WORD_TEMP
        JMS I SHIFT_RIGHT6_PTR
        JMS I SIX_TO_ASCII_PTR
        JMS I PUTCHR_PTR
        CLA
        TAD WORD_TEMP
        AND SIXMASK
        JMS I SIX_TO_ASCII_PTR
        JMS I PUTCHR_PTR
        JMP SIX_LOOP
SIX_DONE,
        JMP I SIXBIT_PAYLOAD

VALIDATE_CODE,
        0
        DCA SIX_INDEX
        CLA
        TAD SIX_INDEX
        SZA
        JMP VC_NONZERO
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE

VC_NONZERO,
        CLA
        TAD SIX_INDEX
        TAD NEG30
        SZA
        JMP VC_OK
        SMA
        JMP VC_OK

        CLA
        TAD SIX_INDEX
        TAD NEG31
        SZA
        JMP VC_OK

        CLA
        TAD SIX_INDEX
        TAD NEG32
        SMA
        JMP VC_BAD

        CLA
        TAD SIX_INDEX
        TAD NEG50
        SPA
        JMP VC_BAD

VC_OK,
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE

VC_BAD,
        ISZ HEADER_BAD
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE

        *1400
PAYLOAD_MSG,
        0120    / P
        0101    / A
        0131    / Y
        0114    / L
        0117    / O
        0101    / A
        0104    / D
        0072    / :
        0040    / space
        0

UNKNOWN_MSG,
        0106    / F
        0117    / O
        0122    / R
        0115    / M
        0101    / A
        0124    / T
        0040    / space
        0125    / U
        0116    / N
        0113    / K
        0116    / N
        0117    / O
        0127    / W
        0116    / N
        0

BAD_HEADER_MSG,
        0102    / B
        0101    / A
        0104    / D
        0040    / space
        0110    / H
        0105    / E
        0101    / A
        0104    / D
        0105    / E
        0122    / R
        0

ASCII_TAG,
        0101    / A
        0123    / S
        0103    / C
        0111    / I
        0111    / I
        0040    / space

BINARY_TAG,
        0102    / B
        0111    / I
        0116    / N
        0101    / A
        0122    / R
        0131    / Y

SIXBIT_TAG,
        0123    / S
        0111    / I
        0130    / X
        0102    / B
        0111    / I
        0124    / T

ASCII_DESC,
        0101    / A
        0123    / S
        0103    / C
        0111    / I
        0111    / I
        0

BINARY_DESC,
        0102    / B
        0111    / I
        0116    / N
        0101    / A
        0122    / R
        0131    / Y
        0

SIXBIT_DESC,
        0123    / S
        0111    / I
        0130    / X
        0102    / B
        0111    / I
        0124    / T
        0

UNKNOWN_DESC,
        0125    / U
        0116    / N
        0113    / K
        0116    / N
        0117    / O
        0127    / W
        0116    / N
        0

MAG_PREFIX,
        0115    / M
        0101    / A
        0107    / G
        0

        *0700
HEADER,
        0
        0
        0
        0
        0
        0

LABEL_BUF,
        0
        0
        0
        0
        0
        0

FORMAT_BUF,
        0
        0
        0
        0
        0
        0

        *0740
CHARMAP,
        0040
        0101
        0102
        0103
        0104
        0105
        0106
        0107
        0110
        0111
        0112
        0113
        0114
        0115
        0116
        0117
        0120
        0121
        0122
        0123
        0124
        0125
        0126
        0127
        0130
        0131
        0132
        0056
        0056
        0056
        0015
        0012
        0060
        0061
        0062
        0063
        0064
        0065
        0066
        0067
        0070
        0071
        0040
        0056
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
        0040
