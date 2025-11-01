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
	CLA CLL
	TAD CONDEOT
	SZA
	JMP DONE	/ EOT means DONE
        ISZ BLKNUM      / next block
        JMP LOOP
DONE,   JMS FFEED
        HLT

CONDEOT,0		/ If non-zero we have reached EOT
WDMODE, 03400
BLKNUM, 1
TAPEWD, 0
N_CR,   07762
NL,     00013
MEOT,   07001             / EOT detect mask
EMASK,	0377

EOTP,	0		/ Test character for EOT
	CLL CLA
	TAD TAPEWD
	AND EMASK	/ 0777
	TAD MEOT	/ 7001
	JMP I EOTP	/ AC is zero if we just read EOT marker

BLKREAD, 0
        IOT 6671        / skip if ready
        JMP I BLKREAD	/ end of block
        IOT 6551        / skip if watchdog expired
        IOT 6554        / restart watchdog
        IOT 6674        / Read next word
        DCA TAPEWD

        JMS PUTCH
	JMS EOTP	/ EOT?
	SZA
	JMP CRP		/ NOT EOT: continue
	CMA
	DCA CONDEOT	/ save it
	JMP I BLKREAD	/ EOT: return
        / add a CR after NL
CRP,    CLA
        TAD TAPEWD
        TAD N_CR
        SZA
        JMP BLKEND
        TAD NL
        JMS PUTCH
BLKEND, JMP BLKREAD     / loop

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
