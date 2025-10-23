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

/ Main read and print loop - don't check ready immediately after select
RDLOOP, IOT 6671                / Skip if paper tape ready
        JMP DONE                / Exit when no more data
        
        IOT 6674                / Read next word from tape into AC
        DCA TAPEWD              / Save the tape word
        
        / Extract and print each 6-bit character from the 12-bit word
        / PDP-8 words are 12 bits, can hold two 6-bit characters
        
        / Print first character (upper 6 bits)
        CLA                     / Clear AC
        TAD TAPEWD              / Load tape word
        RTR RTR RTR RTR RTR RTR / Rotate right 6 times to get upper 6 bits
        AND SIXBIT              / Mask to 6 bits
        JMS PRINTCH             / Print the character
        
        / Print second character (lower 6 bits)  
        CLA                     / Clear AC
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
        DCA CHAR                / Save character
        
        / Convert 6-bit value to printable ASCII
        / 6-bit values 0-63 need to map to reasonable ASCII
        TAD CHAR                / Load character
        SZA                     / Skip if zero
        JMP NOTZERO             / Not zero
        TAD SPACE               / Load space character (ASCII 32)
        JMP PRCHAR              / Go print it
        
NOTZERO, TAD CHAR               / Reload character
        / Simple mapping: add space (32) to get into printable range
        TAD SPACE               / Add 32 to get into ASCII printable range
        AND ASCII7              / Mask to 7 bits for safety
        / This maps: 1->33(!), 2->34("), ..., 42->74(J), etc.

PRCHAR, DCA CHAR                / Save final character

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
ASCII7, 0177                    / 7-bit ASCII mask  
SPACE,  0040                    / ASCII space (32 decimal, 40 octal)

/ Error messages
NOMSG,  NOTEXT
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