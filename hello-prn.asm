/ Print "HELLO" followed by CR/LF using the PDP-8 line printer (device 060).
/
*4000
        CLA CLL
        TAD     MSGADDR
        DCA     PTR             / PTR -> first character
        TAD     MSGLEN
        DCA     COUNT           / COUNT = -N (loop counter)

LOOP,   CLA
        TAD I   PTR             / AC = next character
        DCA     CHAR            / Save for reuse

WAIT,   IOT     6601            / Skip when printer ready
        JMP     WAIT

        CLA
        TAD     CHAR
        IOT     6606            / Clear ready + print character

        ISZ     PTR             / Advance pointer to next byte
        ISZ     COUNT           / Increment counter; skip when done
        JMP     LOOP

DONE,   CLA
        HLT

/ Workspace and data
CHAR,   0000
PTR,    0000
COUNT,  0000
MSGADDR, MESSAGE
MSGLEN, -7                      / Two's complement length (HELLO + CR/LF)

MESSAGE,
        0110                    / 'H'
        0105                    / 'E'
        0114                    / 'L'
        0114                    / 'L'
        0117                    / 'O'
        0015                    / '\r'
        0012                    / '\n'
