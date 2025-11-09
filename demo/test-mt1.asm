/ test-mt1.asm
/ Read the label (first 3 words of header) from magtape unit 1 and print it.
/ Then halt.

        *0010
HDR_PTR,        0
DST_PTR,        0
STR_PTR,        0

        *0020
NEG6,           -6
NEG3,           -3
NEG1,           -1
SIXMASK,        0077
CR,             0015
LF,             0012
COUNT6,         0
UNIT1,          1

WORD_TEMP,      0
CHAR_TEMP,      0
SIX_INDEX,      0
TAB_PTR,        0
CHARMAP_PTR,    CHARMAP
HDR_ADDR,       0050
LBL_ADDR,       0056
WDMODE,         3120

        *0054
HEADER,         0               / Storage for 6-word header
                0
                0
                0
                0
                0

LABEL_BUF,      0               / Storage for decoded label (6 chars)
                0
                0
                0
                0
                0

        *0200
START,  CLA CLL
        TAD WDMODE
        IOT 6552
        CLA CLL
        TAD UNIT1
        IOT 6701                / Select unit 1 (GO)

        / Setup to read 6 header words into HEADER buffer
        CLA CLL
        TAD HDR_ADDR
        TAD NEG1
        DCA HDR_PTR
        CLA CLL
        TAD NEG6
        DCA COUNT6

READ_HDR,
        JMS READWORD
        DCA I HDR_PTR
        ISZ COUNT6
        JMP READ_HDR

        / Decode label into LABEL_BUF (first 3 words)
        CLA CLL
        TAD HDR_ADDR
        TAD NEG1
        DCA HDR_PTR
        CLA CLL
        TAD LBL_ADDR
        TAD NEG1
        DCA DST_PTR
        CLA CLL
        TAD NEG3
        DCA COUNT6

DECODE_LABEL,
        CLA CLL
        TAD I HDR_PTR
        JMS STORE_PAIR
        ISZ COUNT6
        JMP DECODE_LABEL

        / Print label
        CLA CLL
        TAD LBL_ADDR
        TAD NEG1
        DCA STR_PTR
        CLA CLL
        TAD NEG6
        DCA COUNT6
        JMS PRINT_STR
        JMS I PEOL

        HLT

PPCH,	PUTCHR
PEOL,	PRINT_CRLF

        *0300
READWORD,
        0
RD_WAIT,
        IOT 6710                / Skip when word is ready
        JMP RD_WAIT
        IOT 6702                / Read word into AC
        JMP I READWORD

STORE_PAIR,
        0
        DCA WORD_TEMP
        CLA
        TAD WORD_TEMP
        JMS SHIFT_RIGHT6
        JMS SIX_TO_ASCII
        DCA CHAR_TEMP
        CLA
        TAD CHAR_TEMP
        DCA I DST_PTR
        CLA
        TAD WORD_TEMP
        AND SIXMASK
        JMS SIX_TO_ASCII
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
        JMS I PPCH
        JMP PS_LOOP

	*0400
PRINT_CRLF,
        0
        CLA
        TAD CR
        JMS PUTCHR
        CLA
        TAD LF
        JMS PUTCHR
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

        *0500
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
