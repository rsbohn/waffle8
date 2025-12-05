/ unix2dos.asm
/ Convert LF-only UNIX text from the keyboard (KL8E, 603x) into
/ CR/LF-delimited text on the paper tape punch (PTP, 602x).
/ 
/ Behavior:
/   - Reads characters from the keyboard buffer.
/   - Converts ASCII LF (012) into CR (015) + LF.
/   - Stops when EOT (004, Ctrl+D) is received (keyboard/file EOF).
/ 
/ Keyboard (603x):
/   6031 - Skip if ready
/   6032 - Clear flag
/   6036 - Read and clear
/ 
/ Paper Tape Punch (602x):
/   6021 - Skip if ready
/   6026 - Clear flag and punch character (low 8 bits of AC)

        *0020
CHAR,       0               / Masked 8-bit character
OUTCHAR,    0               / Scratch buffer for punch routine
NEG_EOT,    07774           / Two's complement of 0004 (EOT)
NEG_LF,     07766           / Two's complement of 0012
MASK8,      0377
CR_CHAR,    0015
LF_CHAR,    0012
P_PUNCH,    PUNCH

        *0200
START,  CLA CLL

READ_LOOP,
        IOT 6031            / Skip when a character is ready
        JMP READ_LOOP
        IOT 6036            / Read + clear keyboard buffer
        AND MASK8
        DCA CHAR

        CLA
        TAD CHAR
        TAD NEG_EOT
        SZA                 / Zero -> EOT (Ctrl+D)
        JMP CHECK_LF
        JMP FINISHED

CHECK_LF,
        CLA
        TAD CHAR
        TAD NEG_LF
        SZA                 / Zero -> newline detected
        JMP PUNCH_CHAR

        CLA
        TAD CR_CHAR
        JMS I P_PUNCH
        CLA
        TAD LF_CHAR
        JMS I P_PUNCH
        JMP READ_LOOP

PUNCH_CHAR,
        CLA
        TAD CHAR
        JMS I P_PUNCH
        JMP READ_LOOP

FILLER, 0072
FINISHED,
        HLT
        CLL CLA
        TAD FILLER
        IOT 6026
        JMP .-4

/------------------------------------------------------------
/ PUNCH
/   Output low byte of AC to the paper tape punch (PTP).
/------------------------------------------------------------
PUNCH,  0
        DCA OUTCHAR
        IOT 6021            / Skip when punch is ready
        JMP .-1
        TAD OUTCHAR
        IOT 6026            / Clear flag and punch character
        CLA
        CLA
        JMP I PUNCH

        $
