/ mtcp.asm
/ Copy paper tape blocks to magtape unit 1.
/ 
/ This program walks through sequential blocks exposed by the paper tape
/ device (667x) and streams every word to the writable magtape unit (unit 1).
/ Each block is prefixed with a magtape header consisting of a six-character
/ label and format string encoded in SIXBIT (three words each). Blocks are
/ processed in ascending order starting at block 1 by default, unless the
/ console switch register supplies an override on entry.
/ 
/ Usage:
/   1. From the monitor, ensure magtape unit 1 is ready for a fresh record:
/        magtape new 1
/   2. Load and run this program at 0200 (octal).
/   3. The program halts when no further paper tape blocks are available.
/      The resulting magtape record contains a raw copy of the tape words in
/      block order.
/ 
/ Paper Tape Device (667x):
/   6671 - Skip if ready (data available in current block)
/   6672 - Select block (AC = block number)
/   6674 - Read word into AC
/ 
/ Magtape Device (670x):
/   6701 - GO (latch unit number)
/   6704 - WRITE (store AC to tape)
/   6710 - SKIP (skip when device ready)
/   6720 - REWIND (closes current record and refreshes catalog)

        *0010
BLOCKNUM,   0               / Current paper tape block number
TAPEWD,     0               / Latest word read from paper tape
WORD_TEMP,  0               / Scratch for digit extraction
TPCHAR,     0               / Teleprinter character buffer
        *0020
HEADER_LABEL0, 02024        / "PT"
HEADER_LABEL1, 00317        / "CO"
HEADER_LABEL2, 02031        / "PY"
HEADER_FORMAT0, 00123       / "AS"
HEADER_FORMAT1, 00311       / "CI"
HEADER_FORMAT2, 01100       / "I "
UNIT_ONE,   1
FIRST_BLOCK,1
MASK3,      0007
ASCII_ZERO, 0060
SPACE_CHAR, 0040
CR_CHAR,    0015
LF_CHAR,    0012
P_COPY_BLOCK,     COPY_BLOCK
P_WRITE_HEADER,   WRITE_HEADER
P_WRITE_WORD,     WRITE_WORD
P_PRINT_WORD,     PRINT_WORD
P_PRINT_NEWLINE,  PRINT_NEWLINE
P_PUTCH,          PUTCH

        *0200
START,  CLA CLL
        TAD UNIT_ONE
        IOT 6501            / Select magtape unit 1
        IOT 6520            / Ensure previous record (if any) is closed

        CLA CLL             / Optional start block override via switches
        OSR                 / OR switch register into AC
        SNA                 / Skip when switches contain a value
        JMP DEFAULT_BLOCK   / No override: fall back to default
        DCA BLOCKNUM        / Use switches as initial block number
        JMP BEGIN_COPY

DEFAULT_BLOCK,
        CLA CLL
        TAD FIRST_BLOCK
        DCA BLOCKNUM

        JMS I P_WRITE_HEADER / Emit single magtape header at start

BEGIN_COPY,
        CLA CLL
        TAD BLOCKNUM
        IOT 6672            / Select current paper tape block

        IOT 6671            / Skip when data is ready
        JMP FINISH          / Not ready -> no further data

        JMS I P_COPY_BLOCK   / Copy data words

        ISZ BLOCKNUM
        JMP BEGIN_COPY

FINISH,
        IOT 6520            / Flush and close magtape record
        HLT

/------------------------------------------------------------
/ COPY_BLOCK
/   Copies the currently selected paper tape block to magtape and prints it.
/------------------------------------------------------------
COPY_BLOCK, 0
        IOT 6671            / Skip when a word is ready
        JMP CB_NO_DATA      / Not ready -> block unavailable

CB_READ,
        IOT 6674            / Fetch next word from paper tape
        DCA TAPEWD
        JMS I P_PRINT_WORD  / Echo word to teleprinter
        JMS I P_WRITE_WORD  / Send word to magtape

CB_NEXT,
        IOT 6671            / Skip when more data remains in block
        JMP CB_DONE         / End of block reached
        JMP CB_READ         / Ready -> fetch next word

CB_DONE,
        JMS I P_PRINT_NEWLINE / Terminate block with newline
        CLA CLL             / Indicate success
        JMP I COPY_BLOCK

CB_NO_DATA,
        CLA CLL
        IAC                 / Return AC = 1 to signal termination
        JMP I COPY_BLOCK

/------------------------------------------------------------
/ WRITE_WORD
/   Writes the word stored in TAPEWD to magtape.
/------------------------------------------------------------
WRITE_WORD, 0
        CLA CLL
        TAD TAPEWD
        IOT 6504            / Write word to magtape
        JMP I WRITE_WORD

/------------------------------------------------------------
/ WRITE_HEADER
/   Emit label and format header words for the current block.
/------------------------------------------------------------
WRITE_HEADER, 0
        CLA
        TAD HEADER_LABEL0
        DCA TAPEWD
        JMS I P_WRITE_WORD

        CLA
        TAD HEADER_LABEL1
        DCA TAPEWD
        JMS I P_WRITE_WORD

        CLA
        TAD HEADER_LABEL2
        DCA TAPEWD
        JMS I P_WRITE_WORD

        CLA
        TAD HEADER_FORMAT0
        DCA TAPEWD
        JMS I P_WRITE_WORD

        CLA
        TAD HEADER_FORMAT1
        DCA TAPEWD
        JMS I P_WRITE_WORD

        CLA
        TAD HEADER_FORMAT2
        DCA TAPEWD
        JMS I P_WRITE_WORD

        JMP I WRITE_HEADER

        *0340

/------------------------------------------------------------
/ PRINT_NEWLINE
/   Emit CR/LF sequence on the teleprinter.
/------------------------------------------------------------
PRINT_NEWLINE, 0
        CLA
        TAD CR_CHAR
        DCA TPCHAR
        JMS I P_PUTCH
        CLA
        TAD LF_CHAR
        DCA TPCHAR
        JMS I P_PUTCH
        JMP I PRINT_NEWLINE

/------------------------------------------------------------
/ PUTCH
/   Output character stored in TPCHAR to teleprinter (604x).
/------------------------------------------------------------
PUTCH,  0
TP_WAIT,
        IOT 6041            / Skip if teleprinter ready
        JMP TP_WAIT
        CLA
        TAD TPCHAR
        IOT 6046            / Clear flag + print
        JMP I PUTCH

        *0400

/------------------------------------------------------------
/ PRINT_WORD
/   Render TAPEWD as four octal digits followed by a space.
/------------------------------------------------------------
PRINT_WORD, 0
        CLA
        TAD TAPEWD
        DCA WORD_TEMP

        CLA CLL             / Digit 3 (bits 9-11)
        TAD WORD_TEMP
        CLL RAR             / Shift right 3 times
        RAR
        RAR
        CLL RAR             / +3 = 6 total
        RAR
        RAR
        CLL RAR             / +3 = 9 total
        RAR
        RAR
        AND MASK3
        TAD ASCII_ZERO
        DCA TPCHAR
        JMS I P_PUTCH

        CLA CLL             / Digit 2 (bits 6-8)
        TAD WORD_TEMP
        CLL RAR             / Shift right 6 times
        RAR
        RAR
        CLL RAR
        RAR
        RAR
        AND MASK3
        TAD ASCII_ZERO
        DCA TPCHAR
        JMS I P_PUTCH

        CLA CLL             / Digit 1 (bits 3-5)
        TAD WORD_TEMP
        CLL RAR             / Shift right 3 times
        RAR
        RAR
        AND MASK3
        TAD ASCII_ZERO
        DCA TPCHAR
        JMS I P_PUTCH

        CLA CLL             / Digit 0 (bits 0-2)
        TAD WORD_TEMP
        AND MASK3
        TAD ASCII_ZERO
        DCA TPCHAR
        JMS I P_PUTCH

        CLA
        TAD SPACE_CHAR
        DCA TPCHAR
        JMS I P_PUTCH

        JMP I PRINT_WORD
