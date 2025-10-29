/ mt-index.asm
/ Magtape index printer utility.
/ Reads all records from magtape unit 0, decodes each SIXBIT header,
/ and prints an index showing record number, label, and data format.
/ Requires the KL8E console to be attached (for teleprinter output).

        *0010                   / Auto-increment pointers
HDR_PTR,    0
LBL_PTR,    0
FMT_PTR,    0
STR_PTR,    0
DST_PTR,    0

        *0020                   / Zero-page constants and workspace
NEG6,       -6
NEG3,       -3
NEG27,      -27
NEG32,      -32
NEG50,      -50
NEG30,      -30
NEG31,      -31
NEG1,       -1
ONE,        1
DIGIT_BASE, 0060
HEADER_BAD, 0
RECORD_NUM, 0
SIXMASK,    0077
CR,         0015
LF,         0012
NEXT_REC,   0

HEADER_ADDR,    HEADER
LABEL_ADDR,     LABEL_BUF
FORMAT_ADDR,    FORMAT_BUF
TITLE_PTR,      TITLE_MSG
RECORD_MSG_PTR, RECORD_MSG
LABEL_MSG_PTR,  LABEL_MSG
FORMAT_MSG_PTR, FORMAT_MSG
NOHDR_MSG_PTR,  NOHDR_MSG
DONE_MSG_PTR,   DONE_MSG
CHARMAP_PTR,    CHARMAP

WORD_TEMP,  0
CHAR_TEMP,  0
SIX_INDEX,  0
TAB_PTR,    0
WORDS_LEFT, 0
COUNT6,     0
STORE_PAIR_PTR, STORE_PAIR
SHIFT_RIGHT6_PTR, SHIFT_RIGHT6
PRINT_STR_PTR,    PRINT_STR
PRINT_FIXED_PTR,  PRINT_FIXED
PRINT_CRLF_PTR,   PRINT_CRLF
SIX_TO_ASCII_PTR, SIX_TO_ASCII
PUTCHR_PTR,       PUTCHR
PRINT_DEC_PTR,    PRINT_DEC
READWORD_PTR,     READWORD
VALIDATE_CODE_PTR, VALIDATE_CODE
ZERO,           0
NEG_TEN,        -10
NEG_HUNDRED,    -100

        *0200
START,  CLA CLL
        TAD ZERO                / Select unit 0
        IOT 6701                / GO: latch unit number
        IOT 6720                / Rewind to start of tape

        CLA CLL
        TAD TITLE_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR
        JMS I PRINT_CRLF_PTR

        CLA CLL
        DCA RECORD_NUM
        CLA CLL
        DCA NEXT_REC

/ Main record loop
REC_LOOP,
        CLA CLL
        TAD HEADER_ADDR
        TAD NEG1
        DCA HDR_PTR             / HDR_PTR -> HEADER[0]-1 for auto-increment
        CLA CLL
        TAD NEG6
        DCA WORDS_LEFT

READ_HDR,
        JMS I READWORD_PTR            / Read next header word
        SMA                     / Check for end-of-tape (0xFFF)
        JMP REC_DONE            / If negative, we're done
        DCA I HDR_PTR           / Store into HEADER array
        ISZ WORDS_LEFT
        JMP READ_HDR

        / Prepare pointers for decoding
        CLA CLL
        TAD HEADER_ADDR
        TAD NEG1
        DCA HDR_PTR             / Reset header pointer
        CLA CLL
        TAD LABEL_ADDR
        TAD NEG1
        DCA LBL_PTR
        CLA
        TAD LBL_PTR
        DCA DST_PTR
        CLA
        DCA HEADER_BAD

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

        / Output record index line
        CLA CLL
        TAD RECORD_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR

        CLA CLL
        TAD RECORD_NUM
        JMS I PRINT_DEC_PTR

        CLA CLL
        TAD LABEL_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR

        CLA
        TAD HEADER_BAD
        SZA
        JMP BAD_HEADER_LINE

        CLA CLL
        TAD LABEL_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD NEG6
        DCA COUNT6
        JMS I PRINT_FIXED_PTR

        CLA CLL
        TAD FORMAT_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR

        CLA CLL
        TAD FORMAT_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD NEG6
        DCA COUNT6
        JMS I PRINT_FIXED_PTR

        JMS I PRINT_CRLF_PTR

        JMP NEXT_RECORD

BAD_HEADER_LINE,
        CLA CLL
        TAD NOHDR_MSG_PTR
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR

NEXT_RECORD,
        ISZ RECORD_NUM          / Increment record counter
        JMP REC_LOOP            / Read next record

REC_DONE,
        CLA CLL
        TAD DONE_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR

        HLT                     / Done

/------------------------------------------------------------
/ Subroutines
/------------------------------------------------------------

READWORD,
        0
RD_WAIT,
        IOT 6710                / Skip when ready
        JMP RD_WAIT
        IOT 6702                / Read word into AC
        JMP I READWORD_PTR

        *0400
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
        JMP I STORE_PAIR_PTR

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
        JMP I SHIFT_RIGHT6_PTR

VALIDATE_CODE,
        0
        DCA SIX_INDEX
        CLA
        TAD SIX_INDEX
        SZA
        JMP VC_NONZERO
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE_PTR

VC_NONZERO,
        CLA
        TAD SIX_INDEX
        TAD NEG30
        SZA
        JMP VC_OK         / value == 30
        SMA
        JMP VC_OK         / value < 30 (letters, punctuation)

        CLA
        TAD SIX_INDEX
        TAD NEG31
        SZA
        JMP VC_OK         / value == 31

        CLA
        TAD SIX_INDEX
        TAD NEG32
        SMA
        JMP VC_BAD        / (should not happen but guard)

        CLA
        TAD SIX_INDEX
        TAD NEG50
        SPA
        JMP VC_BAD        / value >= 50

VC_OK,
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE_PTR

VC_BAD,
        ISZ HEADER_BAD
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE_PTR

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
        JMP I SIX_TO_ASCII_PTR

PRINT_STR,
        0
PS_LOOP,
        CLA
        TAD I STR_PTR
        SZA                     / Stop on NULL terminator
        JMP PS_CONT
        JMP I PRINT_STR_PTR
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
        JMP I PRINT_FIXED_PTR

PRINT_CRLF,
        0
        CLA
        TAD CR
        JMS I PUTCHR_PTR
        CLA
        TAD LF
        JMS I PUTCHR_PTR
        JMP I PRINT_CRLF_PTR

PRINT_DEC,
        0
        DCA WORD_TEMP
        CLA CLL
        TAD NEG_HUNDRED
        DCA WORDS_LEFT

        CLA
        TAD WORD_TEMP
        TAD NEG_HUNDRED
        SPA
        JMP PD_NOT_HUNDRED
        CLA CLL
        TAD DIGIT_BASE
        IAC                     / +1 for first hundred
        JMS I PUTCHR_PTR
        ISZ WORDS_LEFT
        JMP PD_TENS

PD_NOT_HUNDRED,
        CLA
        TAD WORD_TEMP

PD_TENS,
        DCA WORD_TEMP
        CLA CLL
        TAD NEG_TEN
        DCA WORDS_LEFT

        CLA
        TAD WORD_TEMP
        TAD NEG_TEN
        SPA
        JMP PD_NOT_TEN
        CLA CLL
        TAD DIGIT_BASE
        IAC
        JMS I PUTCHR_PTR
        ISZ WORDS_LEFT
        JMP PD_ONES

PD_NOT_TEN,
        CLA
        TAD WORD_TEMP

PD_ONES,
        DCA WORD_TEMP
        CLA
        TAD WORD_TEMP
        TAD DIGIT_BASE
        JMS I PUTCHR_PTR
        JMP I PRINT_DEC_PTR

PUTCHR,
        0
        DCA CHAR_TEMP
TTY_WAIT,
        IOT 6041                / Teleprinter ready?
        JMP TTY_WAIT
        CLA
        TAD CHAR_TEMP
        IOT 6046                / Transmit character
        JMP I PUTCHR_PTR

        *0600
TITLE_MSG,
        0115    / M
        0101    / A
        0107    / G
        0124    / T
        0101    / A
        0120    / P
        0105    / E
        0040    / space
        0111    / I
        0116    / N
        0104    / D
        0105    / E
        0130    / X
        0
        0

RECORD_MSG,
        0122    / R
        0105    / E
        0103    / C
        0117    / O
        0122    / R
        0104    / D
        0040    / space
        0
        0

LABEL_MSG,
        0040    / space
        0040    / space
        0114    / L
        0101    / A
        0102    / B
        0105    / E
        0114    / L
        0072    / :
        0040    / space
        0
        0

FORMAT_MSG,
        0040    / space
        0040    / space
        0106    / F
        0117    / O
        0122    / R
        0115    / M
        0101    / A
        0124    / T
        0072    / :
        0040    / space
        0
        0

NOHDR_MSG,
        0116    / N
        0117    / O
        0040    / space
        0123    / S
        0111    / I
        0130    / X
        0102    / B
        0111    / I
        0124    / T
        0040    / space
        0110    / H
        0105    / E
        0101    / A
        0104    / D
        0105    / E
        0122    / R
        0
        0

DONE_MSG,
        0111    / I
        0116    / N
        0104    / D
        0105    / E
        0130    / X
        0040    / space
        0103    / C
        0117    / O
        0115    / M
        0120    / P
        0114    / L
        0105    / E
        0124    / T
        0105    / E
        0
        0

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

        *0740               / Keep CHARMAP away from buffers
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
        0040
        0040
        0040
        0040
        0040
        0040
        0040
