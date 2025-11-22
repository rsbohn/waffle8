/ Inventory bootstrap helpers for the dry goods system.
/ Page 2 driver loop plus helpers for console echo and tape dumping.

*0200
START,		JMS I _SUBR
			HLT
			JMP 0200
_SUBR,		0400

*0400

/ MAIN — Read characters, echo them, and service control commands.
MAIN,       CLA CLL
            DCA     LASTCHAR

MAIN_LOOP,
            JMS     TTYECHO        / Echo keyboard input, AC holds byte
            DCA     LASTCHAR

            CLA
            TAD     LASTCHAR
            TAD     NEG_CTRL_P     / Ctrl-P (020) prints the mounted tape
            SZA
            JMP     MAIN_LOOP

            JMS     PTPRINT
            JMP     MAIN_LOOP

/ PTPRINT — Stream ASCII tape contents to the teleprinter until 0777.
/ Usage: JMS PTPRINT
PTPRINT,    0
            CLA CLL
            TAD     ONE             / Block numbers start at 0001
            DCA     PTBLK

PTBLKSEL,   CLA
            TAD     PTBLK
            IOT     6672            / SELECT block N
            IOT     6671            / Ready? (skip on ready)
            JMP     PTDONE          / No more blocks loaded

PTREAD,     IOT     6671            / Skip while data remains in block
            JMP     PTNEXT          / Block finished, advance
            IOT     6674            / READ next word into AC
            DCA     PTWORD

            CLA
            TAD     PTWORD
            TAD     NEG_EOT         / Word == 0777 ?
            SZA
            JMP     PTEMIT
            JMP     PTDONE          / End-of-tape marker reached

PTEMIT,     CLA
            TAD     PTWORD
            AND     MASK377         / Low 8 bits contain ASCII
            JMS     TTYOUT
            JMP     PTREAD

PTNEXT,     CLA
            TAD     PTBLK
            TAD     ONE
            DCA     PTBLK
            JMP     PTBLKSEL

PTDONE,     CLA
            JMP I   PTPRINT

/ TTYOUT — Write character (AC) to KL8E teleprinter, preserving AC.
TTYOUT,     0
            DCA     CHARBUF
TTYWAIT,    IOT     6041            / TSF: skip when ready
            JMP     TTYWAIT
            CLA
            TAD     CHARBUF
            IOT     6046            / TLS: clear flag + load
            CLA
            JMP I   TTYOUT

/ TTYGET — Wait for keyboard character, returns ASCII in AC.
TTYGET,     0
KBDWAIT,    IOT     6031            / KSF: skip when key ready
            JMP     KBDWAIT
            IOT     6036            / KRB: clear flag + read character
            JMP I   TTYGET

/ TTYECHO — Read a character, echo to teleprinter, leave char in AC.
TTYECHO,    0
            JMS     TTYGET
            DCA     ECHOTMP
            CLA
            TAD     ECHOTMP
            JMS     TTYOUT
            CLA
            TAD     ECHOTMP
            JMP I   TTYECHO

/ Workspace and constants (still within page 2).
PTBLK,      0000
PTWORD,     0000
CHARBUF,    0000
ECHOTMP,    0000
LASTCHAR,   0000
ONE,        0001
MASK377,    0377
NEG_EOT,    07001                   / Two's complement of 0777
NEG_CTRL_P, 07760                   / -020 (Ctrl-P) for equality test
