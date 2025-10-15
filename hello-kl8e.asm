/ KL8E line echo: type a line, press return, see it printed back with CR/LF.
/ Assembles at 04000 and uses the keyboard (device 03) and teleprinter (device 04).

*4000
START,  CLA CLL
        TAD     BUFADDR         / WRITE_PTR <- buffer start
        DCA     WRITE_PTR
        CLA
        DCA     LEN             / No characters buffered yet

READCHR,
        IOT     6031            / KSF: skip when keyboard ready
        JMP     READCHR
        IOT     6036            / KRB: clear flag + load character into AC
        DCA     CHAR

        CLA
        TAD     CHAR
        TAD     NEG_CR
        SZA                     / Was it carriage return?
        JMP     STORE_CHAR
        JMP     FLUSH_LINE      / CR triggers line output

STORE_CHAR,
        CLA
        TAD     CHAR
        DCA I   WRITE_PTR       / Store character into buffer
        ISZ     WRITE_PTR       / Advance buffer pointer

        CLA
        TAD     LEN
        TAD     ONE
        DCA     LEN             / Increment buffered length

        CLA
        TAD     LEN
        TAD     NEG_BUF_MAX
        SZA                     / Buffer full? (LEN == BUF_MAX)
        JMP     READCHR
        JMP     FLUSH_LINE

FLUSH_LINE,
        CLA
        TAD     BUFADDR
        DCA     READ_PTR        / READ_PTR <- buffer start

FLUSH_LOOP,
        CLA
        TAD     LEN
        SZA                     / Any characters left?
        JMP     OUTPUT_CRLF

        CLA
        TAD I   READ_PTR        / Load next buffered character
        DCA     CHAR

TTY_WAIT,
        IOT     6041            / TSF: skip when teleprinter ready
        JMP     TTY_WAIT

        CLA
        TAD     CHAR
        IOT     6046            / TLS with clear flag

        ISZ     READ_PTR        / Advance read pointer

        CLA
        TAD     LEN
        TAD     NEG1
        DCA     LEN             / Decrement remaining character count
        JMP     FLUSH_LOOP

OUTPUT_CRLF,
        CLA
        TAD     CR
        DCA     CHAR

TTY_WAIT_CR,
        IOT     6041
        JMP     TTY_WAIT_CR

        CLA
        TAD     CHAR
        IOT     6046

        CLA
        TAD     LF
        DCA     CHAR

TTY_WAIT_LF,
        IOT     6041
        JMP     TTY_WAIT_LF

        CLA
        TAD     CHAR
        IOT     6046

        CLA                     / Reset buffer length and pointer
        DCA     LEN
        CLA
        TAD     BUFADDR
        DCA     WRITE_PTR
        JMP     READCHR

/ Workspace and constants -------------------------------------------------

CHAR,       0000                / Last character processed
WRITE_PTR,  0000                / Pointer to next free slot in buffer
READ_PTR,   0000                / Pointer while draining buffer
LEN,        0000                / Number of characters currently buffered

BUFADDR,    BUFFER              / Address constant for buffer base

CR,         0015
LF,         0012
ONE,        0001
NEG1,       7777                / -1 (twos-complement)
NEG_CR,     07763               / -CR for equality test
NEG_BUF_MAX,07700               / -BUF_MAX (BUF_MAX = 64)

/ 64-word line buffer at 04120.
BUFFER,
            .BLKW   100

        END START
