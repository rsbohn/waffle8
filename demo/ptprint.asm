/ ptprint.asm
/ Copy paper tape contents to line printer
/ 
/ This program reads data from the paper tape device and prints it
/ to the line printer. The paper tape file is specified in the emulator
/ configuration (pdp8.config).
/
/ Usage:
/   1. Configure paper tape image in pdp8.config:
/      device paper_tape {
/        iot = 667x
/        image = tapes/lorem.tape
/      }
/   2. Load and run this program at address 200 (octal)
/   3. The program will read block 1 from the tape and print each
/      6-bit character to the line printer, converting SIXBIT encoding
/      to printable ASCII characters
/
/ Paper Tape Device (667x):
/   6671 - Skip if ready
/   6672 - Select block (AC = block number)
/   6674 - Read word into AC
/
/ Line Printer Device (660x):
/   6601 - Skip if printer ready
/   6602 - Clear ready flag
/   6604 - Print character (low 7 bits of AC)
/   6606 - Clear ready + print character

        *200                    / Start at location 200 (octal)

START,  CLA CLL                 / Clear AC and Link
        TAD BLKNUM              / Load block number to read
        IOT 6672                / Select block on paper tape
        JMS RDLOOP
        ISZ BLKNUM              / Increment block number for next read
	JMP START
	7402
        JMP DONE

/ Main read and print loop - don't check ready immediately after select
RDLOOP, 0
        IOT 6671                / Skip if paper tape ready
        JMP I RDLOOP            / return
        
        IOT 6674                / Read next word from tape into AC
        DCA TAPEWD              / Save the tape word
        
        / Extract and print each 6-bit character from the 12-bit word
        / PDP-8 words are 12 bits, can hold two 6-bit characters
        
        / Print first character (upper 6 bits)
        CLA CLL                 / Clear AC and Link
        TAD TAPEWD              / Load tape word
        RAR; RAR; RAR           / Rotate right 6 positions total
        RAR; RAR; RAR
        AND SIXBIT              / Mask to 6 bits
        JMS PRINTCH             / Print the character

        / Print second character (lower 6 bits)
        CLA CLL                 / Clear AC and Link
        TAD TAPEWD              / Load tape word
        AND SIXBIT              / Mask to lower 6 bits
        JMS PRINTCH             / Print the character
        
        JMP RDLOOP              / Continue reading

DONE,   CLA                     / Clear AC
        HLT                     / Halt program

NOTAPE, CLA                     / Clear AC
        TAD NOMSG               / Load "NO TAPE" message pointer
        DCA MSGPTR              / Set message pointer
        JMS PRNMSG              / Print error message
        HLT                     / Halt

/------------------------------------------------------------
/ PRINTCH - Print a single 6-bit character to line printer
/ Entry: AC contains 6-bit character code
/ Exit:  Character printed, AC preserved
/------------------------------------------------------------
PRINTCH, 0                      / Return address
        DCA CHAR                / Save SIXBIT character value

        / Convert SIXBIT code to ASCII via lookup table
        CLA
        TAD CHARMAP_PTR         / Load base address of conversion table
        DCA TABPTR
        CLA
        TAD CHAR                / AC = index within table (0-63)
        TAD TABPTR              / Add base address to index
        DCA TABPTR              / TABPTR -> table entry
        CLA
        TAD I TABPTR            / Fetch ASCII translation
        DCA CHAR                / Save final character

/ Wait for printer ready and print
PRWAIT, IOT 6601                / Skip if printer ready
        JMP PRWAIT              / Wait until ready
        
        CLA                     / Clear AC
        TAD CHAR                / Load character to print
        IOT 6606                / Clear ready flag and print character
        
        JMP I PRINTCH           / Return

/------------------------------------------------------------
/ PRNMSG - Print null-terminated message to line printer
/ Entry: MSGPTR points to message
/------------------------------------------------------------
PRNMSG, 0                       / Return address
        
MSGLOOP, CLA                    / Clear AC
        TAD I MSGPTR            / Load next character
        SZA                     / Skip if null terminator
        JMP PRNCH               / Print character
        JMP I PRNMSG            / Return when done
        
PRNCH,  JMS PRINTCH             / Print the character
        ISZ MSGPTR              / Advance to next character
        JMP MSGLOOP             / Continue

/------------------------------------------------------------
/ Data and workspace
/------------------------------------------------------------
BLKNUM, 1                       / Block number to read (start with block 1)
TAPEWD, 0                       / Current tape word
CHAR,   0                       / Current character being processed
MSGPTR, 0                       / Message pointer for error messages

/ Constants for character conversion
SIXBIT, 0077                    / 6-bit mask (octal 77 = decimal 63)
SPACE,  0040                    / ASCII space (32 decimal, 40 octal)

NOMSG,  NOTEXT                  / Pointer to "NO TAPE" message

/ SIXBIT to ASCII conversion table (indices 0-63)
CHARMAP_PTR, CHARMAP            / Pointer to lookup table
TABPTR, 0                       / Scratch pointer for table indexing

CHARMAP,
        0040                    / 00 -> space
        0101                    / 01 -> A
        0102                    / 02 -> B
        0103                    / 03 -> C
        0104                    / 04 -> D
        0105                    / 05 -> E
        0106                    / 06 -> F
        0107                    / 07 -> G
        0110                    / 08 -> H
        0111                    / 09 -> I
        0112                    / 10 -> J
        0113                    / 11 -> K
        0114                    / 12 -> L
        0115                    / 13 -> M
        0116                    / 14 -> N
        0117                    / 15 -> O
        0120                    / 16 -> P
        0121                    / 17 -> Q
        0122                    / 18 -> R
        0123                    / 19 -> S
        0124                    / 20 -> T
        0125                    / 21 -> U
        0126                    / 22 -> V
        0127                    / 23 -> W
        0130                    / 24 -> X
        0131                    / 25 -> Y
        0132                    / 26 -> Z
        0056                    / 27 -> '.' (reserved)
        0056                    / 28 -> '.' (reserved)
        0056                    / 29 -> '.' (reserved)
        0015                    / 30 -> carriage return
        0012                    / 31 -> line feed
        0060                    / 32 -> '0'
        0061                    / 33 -> '1'
        0062                    / 34 -> '2'
        0063                    / 35 -> '3'
        0064                    / 36 -> '4'
        0065                    / 37 -> '5'
        0066                    / 38 -> '6'
        0067                    / 39 -> '7'
        0070                    / 40 -> '8'
        0071                    / 41 -> '9'
        0041                    / 42 -> '!'
        0054                    / 43 -> ','
        0055                    / 44 -> '-'
        0056                    / 45 -> '.'
        0047                    / 46 -> '\''
        0072                    / 47 -> ':'
        0073                    / 48 -> ';'
        0077                    / 49 -> '?'
        0056                    / 50 -> '.' (undefined)
        0056                    / 51 -> '.' (undefined)
        0056                    / 52 -> '.' (undefined)
        0056                    / 53 -> '.' (undefined)
        0056                    / 54 -> '.' (undefined)
        0056                    / 55 -> '.' (undefined)
        0056                    / 56 -> '.' (undefined)
        0056                    / 57 -> '.' (undefined)
        0056                    / 58 -> '.' (undefined)
        0056                    / 59 -> '.' (undefined)
        0056                    / 60 -> '.' (undefined)
        0056                    / 61 -> '.' (undefined)
        0056                    / 62 -> '.' (undefined)
        0056                    / 63 -> '.' (undefined)

/ Error messages
NOTEXT, 0116                    / 'N'
        0117                    / 'O'
        0040                    / ' '
        0124                    / 'T'
        0101                    / 'A'
        0120                    / 'P'
        0105                    / 'E'
        0015                    / CR
        0012                    / LF
        0000                    / Null terminator
