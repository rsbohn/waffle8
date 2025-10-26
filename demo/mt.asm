/ mt.asm
/ Magtape header inspection utility.
/ Selects magtape unit 0, reads the six-word SIXBIT header, decodes the
/ label and data-format fields, and prints them to the teleprinter.
/ Requires the KL8E console to be attached (for TLS output).

        *0010                   / Auto-increment pointers
HDR_PTR,    0
LBL_PTR,    0
FMT_PTR,    0
STR_PTR,    0
DST_PTR,    0

        *0020                   / Zero-page constants and workspace
NEG6,       -6
NEG3,       -3
UNIT_MASK,  0007
NEG27,      -27
NEG32,      -32
NEG50,      -50
NEG30,      -30
NEG31,      -31
NEG52,      -52
NEG1,       -1
ONE,        1
DIGIT_BASE, 0060
HEADER_BAD, 0
UNIT_NUM,   0
SIXMASK,    0077
CR,         0015
LF,         0012

HEADER_ADDR,    HEADER
LABEL_ADDR,     LABEL_BUF
FORMAT_ADDR,    FORMAT_BUF
TITLE_PTR,      TITLE_MSG
UNIT_MSG_PTR,   UNIT_MSG
LABEL_MSG_PTR,  LABEL_MSG
FORMAT_MSG_PTR, FORMAT_MSG
NOHDR_MSG_PTR,  NOHDR_MSG
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

        *0200
START,  CLA CLL
        OSR                     / Read switch register for unit selection
        AND UNIT_MASK
        DCA UNIT_NUM

        CLA CLL
        TAD UNIT_NUM
        IOT 6701                / GO: latch unit number
        IOT 6720                / Rewind to start of tape

        CLA CLL
        TAD HEADER_ADDR
        DCA HDR_PTR             / HDR_PTR -> HEADER[0]
        CLA CLL
        TAD NEG6
        DCA WORDS_LEFT

READ_HDR,
        JMS READWORD            / Read next header word
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

        / Output header banner and decoded text
        CLA CLL
        TAD TITLE_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR

        CLA CLL
        TAD UNIT_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        CLA
        TAD UNIT_NUM
        TAD DIGIT_BASE
        DCA CHAR_TEMP
        JMS I PUTCHR_PTR
        JMS I PRINT_CRLF_PTR

        CLA
        TAD HEADER_BAD
        SZA
        JMP BAD_HEADER

        CLA CLL
        TAD LABEL_MSG_PTR
        TAD NEG1
        DCA STR_PTR
        JMS I PRINT_STR_PTR

        CLA CLL
        TAD LABEL_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD NEG6
        DCA COUNT6
        JMS I PRINT_FIXED_PTR
        JMS I PRINT_CRLF_PTR

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

        HLT                     / Done

BAD_HEADER,
        CLA CLL
        TAD NOHDR_MSG_PTR
        DCA STR_PTR
        JMS I PRINT_STR_PTR
        JMS I PRINT_CRLF_PTR
        HLT

/------------------------------------------------------------
/ Subroutines
/------------------------------------------------------------

READWORD,
        0
RD_WAIT,
        IOT 6710                / Skip when ready
        JMP RD_WAIT
        IOT 6702                / Read word into AC
        JMP I READWORD

        *0400
STORE_PAIR,
        0
        DCA WORD_TEMP
        CLA
        TAD WORD_TEMP
        JMS I SHIFT_RIGHT6_PTR
        JMS VALIDATE_CODE
        JMS I SIX_TO_ASCII_PTR
        DCA CHAR_TEMP
        CLA
        TAD CHAR_TEMP
        DCA I DST_PTR
        CLA
        TAD WORD_TEMP
        AND SIXMASK
        JMS VALIDATE_CODE
        JMS I SIX_TO_ASCII_PTR
        DCA CHAR_TEMP
        CLA
        TAD CHAR_TEMP
        DCA I DST_PTR
        JMP I STORE_PAIR

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
        JMP I VALIDATE_CODE

VC_BAD,
        ISZ HEADER_BAD
        CLA
        TAD SIX_INDEX
        JMP I VALIDATE_CODE

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
        SZA                     / Stop on NULL terminator
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
TTY_WAIT,
        IOT 6041                / Teleprinter ready?
        JMP TTY_WAIT
        CLA
        TAD CHAR_TEMP
        IOT 6046                / Transmit character
        JMP I PUTCHR

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
        0110    / H
        0105    / E
        0101    / A
        0104    / D
        0105    / E
        0122    / R
        0

UNIT_MSG,
        0125    / U
        0116    / N
        0111    / I
        0124    / T
        0040    / space
        0040    / space
        0072    / :
        0040    / space
        0

LABEL_MSG,
        0114    / L
        0101    / A
        0102    / B
        0105    / E
        0114    / L
        0072    / :
        0040    / space
        0

FORMAT_MSG,
        0106    / F
        0117    / O
        0122    / R
        0115    / M
        0101    / A
        0124    / T
        0072    / :
        0040    / space
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
        0040    / space
        0104    / D
        0105    / E
        0124    / T
        0105    / E
        0103    / C
        0124    / T
        0105    / E
        0104    / D
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

        *0700
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
