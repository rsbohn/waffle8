/ ascii-print.asm
/ Read ASCII-encoded paper tape and print to line printer
/ 
/ This program reads ASCII data from the paper tape device where each
/ 12-bit word contains one ASCII character in the low 8 bits, and prints
/ it to the line printer.
/
/ Usage:
/   1. Configure paper tape image in pdp8.config:
/      device paper_tape {
/        iot = 667x
/        image = tapes/8asm-man.tape
/      }
/   2. Load and run this program at address 200 (octal)
/   3. The program will read from the tape and print each ASCII character
/
/ Paper Tape Device (667x):
/   6671 - Skip if ready
/   6672 - Select block (AC = block number)
/   6674 - Read word into AC
/
/ Line Printer Device (660x):
/   6601 - Skip if printer ready
/   6606 - Clear ready + print character

        *200                    / Start at location 200 (octal)

START,  CLA CLL                 / Clear AC and Link
        TAD BLKNUM              / Load block number to read
        IOT 6672                / Select block on paper tape
        
RDLOOP, IOT 6671                / Skip if paper tape ready
        JMP DONE                / No more data, we're done
        
        IOT 6674                / Read next word from tape into AC
        AND MASK8               / Mask to 8 bits (ASCII range)
        SZA                     / Skip if zero (end marker)
        JMS PRINTCH             / Print the character
        
        JMP RDLOOP              / Continue reading

DONE,   CLA                     / Clear AC
        HLT                     / Halt program

/------------------------------------------------------------
/ PRINTCH - Print a single ASCII character to line printer
/ Entry: AC contains ASCII character code
/ Exit:  Character printed, AC preserved
/------------------------------------------------------------
PRINTCH, 0                      / Return address
        DCA CHAR                / Save character

PRWAIT, IOT 6601                / Skip if printer ready
        JMP PRWAIT              / Wait until ready
        
        CLA                     / Clear AC
        TAD CHAR                / Load character to print
        IOT 6606                / Clear ready flag and print character
        
        JMP I PRINTCH           / Return

/------------------------------------------------------------
/ Data and workspace
/------------------------------------------------------------
BLKNUM, 1                       / Block number to read (start with block 1)
CHAR,   0                       / Current character being processed
MASK8,  0377                    / 8-bit mask (255 decimal, 377 octal)

$
