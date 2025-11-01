/ print tape (ascii)
/ Paper Tape Device (667x):
/   6671 - Skip if ready
/   6672 - Select block (AC = block number)
/   6674 - Read word into AC
/
/ Tele-printer (604x):
/   6041 - Skip if 'ready'
/   6042 - Clear ready flag
/   6044 - Print character (AC & 0177)
/   6046 - Clear flag and print

        *0200
START,  CLA CLL
        TAD WDMODE
        IOT 6552
LOOP,   CLA CLL
        TAD BLKNUM
        IOT 6672        / select block
        JMS BLKREAD     / read and print block
        ISZ BLKNUM      / next block
        JMP LOOP
DONE,   JMS FFEED
        HLT

WDMODE, 03400
BLKNUM, 1
TAPEWD, 0
N_CR,   07762
NL,     00013
EOT,    07777             / EOT detected (got busy twice from IOT 6671)

BLKREAD, 0
        IOT 6671        / skip if ready
        JMP PTBUSY     / check for double /ready
        IOT 6551        / skip if watchdog expired
        IOT 6554        / restart watchdog
        IOT 6674        / Read next word
        DCA TAPEWD

        JMS PUTCH
        / add a CR after NL
        CLA
        TAD TAPEWD
        TAD N_CR
        SZA
        JMP BLKEND
        TAD NL
        JMS PUTCH
BLKEND, JMP BLKREAD     / loop

STAR,    0052
PTBUSY,
        CLA CLL         / PTBUSY
        TAD STAR
        DCA TAPEWD
        JMS PUTCH
        IOT 6671        / try again
        ISZ EOT         / we just want to increment EOT from -1 to zero.
        JMP DONE
        CLA CLL         / reset EOT
        CMA             / make it 07777
        DCA EOT
        JMP I BLKREAD   / just one -- continue

FFEED,  0
        CLL CLA
        TAD FFCHAR
        DCA TAPEWD
        JMS PUTCH
        JMP I FFEED


MASK,   0177
FFCHAR, 0014            / 0001100 FORMFEED

PUTCH,  0
SPIN,   IOT 6041        / skip if teleprinter ready
        JMP SPIN
        CLA
        TAD TAPEWD
        IOT 6046
        JMP I PUTCH     / RETURN